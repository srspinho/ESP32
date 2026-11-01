#include "usb_teclado.h"
#include "placa.h"
#include "log.h"

#include <string.h>
#include <freertos/task.h>
#include <esp_intr_alloc.h>
#include <usb/usb_host.h>

/* =====================================================================
   ESTADO
   ===================================================================== */
struct Interface {
  bool            ativa;
  bool            teclado_boot;   // classe HID, subclasse boot, protocolo teclado
  uint8_t         numero;         // bInterfaceNumber
  uint8_t         endpoint;       // endpoint de interrupcao IN
  uint16_t        tam_pacote;     // wMaxPacketSize
  usb_transfer_t* xfer;
  volatile bool   em_voo;         // transferencia submetida e ainda nao devolvida
};

struct Dispositivo {
  usb_device_handle_t hdl;
  bool                aberto;
  bool                saindo;     // desmontando: nao resubmeter transferencias
  uint16_t            vid, pid;
  uint8_t             n_itf;
  Interface           itf[USB_MAX_INTERFACES];
  usb_transfer_t*     ctrl;
  volatile bool       ctrl_ocupado;
  // Pedidos de controle pendentes (um por vez no endpoint 0)
  uint8_t             protocolo_pendente;   // bits: interfaces que precisam de SET_PROTOCOL
  bool                leds_pendente;
};

static QueueHandle_t            s_fila = nullptr;
static usb_host_client_handle_t s_cliente = nullptr;
static Dispositivo              s_disp;

// Eventos do callback do cliente, tratados na tarefa
static volatile uint8_t s_novo_endereco = 0;   // 0 = nenhum
static volatile bool    s_saiu = false;

static volatile uint8_t s_leds = 0;
static volatile bool    s_conectado = false;
static char             s_resumo[32] = "-";

/* =====================================================================
   PEDIDOS DE CONTROLE (endpoint 0)
   ===================================================================== */
static void cb_controle(usb_transfer_t* t) {
  if (t->status != USB_TRANSFER_STATUS_COMPLETED)
    log_printf("[USB] pedido de controle falhou (status %d)\n", (int)t->status);
  s_disp.ctrl_ocupado = false;
}

// Pedido de classe HID para uma interface (sem fase de dados de entrada)
static bool enviar_controle(uint8_t pedido, uint16_t valor, uint8_t itf,
                            const uint8_t* dados, uint16_t n) {
  Dispositivo& d = s_disp;
  if (!d.ctrl || d.ctrl_ocupado) return false;

  usb_setup_packet_t* s = (usb_setup_packet_t*)d.ctrl->data_buffer;
  s->bmRequestType = 0x21;          // host->dispositivo, classe, interface
  s->bRequest      = pedido;
  s->wValue        = valor;
  s->wIndex        = itf;
  s->wLength       = n;
  if (n) memcpy(d.ctrl->data_buffer + sizeof(usb_setup_packet_t), dados, n);

  d.ctrl->device_handle    = d.hdl;
  d.ctrl->bEndpointAddress = 0;
  d.ctrl->callback         = cb_controle;
  d.ctrl->context          = nullptr;
  d.ctrl->num_bytes        = sizeof(usb_setup_packet_t) + n;

  d.ctrl_ocupado = true;
  if (usb_host_transfer_submit_control(s_cliente, d.ctrl) != ESP_OK) {
    d.ctrl_ocupado = false;
    return false;
  }
  return true;
}

static constexpr uint8_t HID_SET_REPORT   = 0x09;
static constexpr uint8_t HID_SET_IDLE     = 0x0A;
static constexpr uint8_t HID_SET_PROTOCOL = 0x0B;

// Um pedido por chamada; o proximo sai quando o anterior terminar.
static void processar_pendencias() {
  Dispositivo& d = s_disp;
  if (!d.aberto || d.saindo || d.ctrl_ocupado) return;

  for (uint8_t i = 0; i < d.n_itf; i++) {
    if (!(d.protocolo_pendente & (1u << i))) continue;
    // SET_PROTOCOL(0 = boot): o teclado manda sempre os 8 bytes padrao
    if (enviar_controle(HID_SET_PROTOCOL, 0, d.itf[i].numero, nullptr, 0))
      d.protocolo_pendente &= ~(1u << i);
    return;
  }

  if (d.leds_pendente) {
    for (uint8_t i = 0; i < d.n_itf; i++) {
      if (!d.itf[i].teclado_boot) continue;
      const uint8_t leds = s_leds;
      // SET_REPORT(Output, ID 0): 1 byte com os LEDs
      if (enviar_controle(HID_SET_REPORT, 0x0200, d.itf[i].numero, &leds, 1))
        d.leds_pendente = false;
      return;
    }
    d.leds_pendente = false;   // nenhuma interface de teclado
  }
}

/* =====================================================================
   RELATORIOS (interrupcao IN)
   ===================================================================== */
static void cb_relatorio(usb_transfer_t* t) {
  Interface* itf = (Interface*)t->context;

  if (t->status == USB_TRANSFER_STATUS_COMPLETED) {
    const int n = t->actual_num_bytes;
    if (USB_DEPURAR && n > 0) {
      char hex[3 * 16 + 1] = { 0 };
      for (int i = 0; i < n && i < 16; i++) snprintf(hex + 3 * i, 4, "%02X ", t->data_buffer[i]);
      log_printf("[USB] itf %u, %d bytes: %s\n", itf->numero, n, hex);
    }
    if (itf->teclado_boot && n == 8 && s_fila) {
      RelatorioTeclado r;
      memcpy(r.dados, t->data_buffer, 8);
      xQueueSend(s_fila, &r, 0);   // fila cheia: descarta (nunca trava o USB)
    }
  } else if (t->status != USB_TRANSFER_STATUS_CANCELED &&
             t->status != USB_TRANSFER_STATUS_NO_DEVICE) {
    log_printf("[USB] itf %u: erro na leitura (status %d)\n", itf->numero, (int)t->status);
  }

  // Continua lendo enquanto o teclado estiver montado
  const bool continuar = s_disp.aberto && !s_disp.saindo &&
                         (t->status == USB_TRANSFER_STATUS_COMPLETED ||
                          t->status == USB_TRANSFER_STATUS_TIMED_OUT);
  if (continuar && usb_host_transfer_submit(t) == ESP_OK) return;
  itf->em_voo = false;
}

/* =====================================================================
   MONTAR / DESMONTAR
   ===================================================================== */
static void desmontar() {
  Dispositivo& d = s_disp;
  if (!d.aberto) return;
  d.saindo = true;

  // Cancela as leituras em andamento e espera os callbacks devolverem
  for (uint8_t i = 0; i < d.n_itf; i++) {
    Interface& itf = d.itf[i];
    if (!itf.ativa || !itf.em_voo) continue;
    usb_host_endpoint_halt(d.hdl, itf.endpoint);
    usb_host_endpoint_flush(d.hdl, itf.endpoint);
  }
  for (int volta = 0; volta < 100; volta++) {
    bool ocupado = d.ctrl_ocupado;
    for (uint8_t i = 0; i < d.n_itf; i++) ocupado |= d.itf[i].em_voo;
    if (!ocupado) break;
    usb_host_client_handle_events(s_cliente, pdMS_TO_TICKS(10));
  }

  for (uint8_t i = 0; i < d.n_itf; i++) {
    Interface& itf = d.itf[i];
    if (!itf.ativa) continue;
    usb_host_endpoint_clear(d.hdl, itf.endpoint);
    if (itf.xfer) usb_host_transfer_free(itf.xfer);
    usb_host_interface_release(s_cliente, d.hdl, itf.numero);
  }
  if (d.ctrl) usb_host_transfer_free(d.ctrl);
  usb_host_device_close(s_cliente, d.hdl);

  memset(&d, 0, sizeof(d));
  s_conectado = false;
  snprintf(s_resumo, sizeof(s_resumo), "-");

  // Solta todas as teclas no PC (o teclado saiu com tecla apertada?)
  if (s_fila) {
    RelatorioTeclado r = {};
    xQueueSend(s_fila, &r, 0);
  }
  log_printf("[USB] teclado desconectado\n");
}

static void montar(uint8_t endereco) {
  Dispositivo& d = s_disp;
  if (d.aberto) {
    log_printf("[USB] ja ha um teclado; dispositivo %u ignorado (sem suporte a hub)\n", endereco);
    return;
  }
  memset(&d, 0, sizeof(d));

  if (usb_host_device_open(s_cliente, endereco, &d.hdl) != ESP_OK) {
    log_printf("[USB] falha ao abrir o dispositivo %u\n", endereco);
    return;
  }

  const usb_device_desc_t* dd = nullptr;
  const usb_config_desc_t* cfg = nullptr;
  if (usb_host_get_device_descriptor(d.hdl, &dd) != ESP_OK ||
      usb_host_get_active_config_descriptor(d.hdl, &cfg) != ESP_OK) {
    usb_host_device_close(s_cliente, d.hdl);
    return;
  }
  d.vid = dd->idVendor;
  d.pid = dd->idProduct;
  if (dd->bDeviceClass == 0x09) {
    log_printf("[USB] %04X:%04X e um hub: ainda nao suportado\n", d.vid, d.pid);
    usb_host_device_close(s_cliente, d.hdl);
    return;
  }

  // Percorre o descritor de configuracao: interfaces HID e seus endpoints
  const uint8_t* p = (const uint8_t*)cfg;
  const uint16_t total = cfg->wTotalLength;
  Interface* atual = nullptr;
  for (uint16_t i = 0; i + 2 <= total && p[i] >= 2; i += p[i]) {
    const uint8_t tam = p[i], tipo = p[i + 1];
    if (i + tam > total) break;

    if (tipo == 0x04 && tam >= 9) {                 // INTERFACE
      atual = nullptr;
      const uint8_t num = p[i + 2], alt = p[i + 3];
      const uint8_t classe = p[i + 5], sub = p[i + 6], proto = p[i + 7];
      if (classe != 0x03 || alt != 0 || d.n_itf >= USB_MAX_INTERFACES) continue;
      atual = &d.itf[d.n_itf];
      memset(atual, 0, sizeof(*atual));
      atual->numero = num;
      atual->teclado_boot = (sub == 0x01 && proto == 0x01);
      log_printf("[USB] itf %u: HID subclasse %u protocolo %u%s\n", num, sub, proto,
                 atual->teclado_boot ? " (teclado boot)" : "");
    } else if (tipo == 0x05 && tam >= 7 && atual && !atual->endpoint) {   // ENDPOINT
      const uint8_t end = p[i + 2], attr = p[i + 3];
      if ((end & 0x80) && (attr & 0x03) == 0x03) {  // interrupcao IN
        atual->endpoint   = end;
        atual->tam_pacote = (uint16_t)(p[i + 4] | (p[i + 5] << 8)) & 0x7FF;
        d.n_itf++;
        atual = nullptr;
      }
    }
  }

  // Reivindica as interfaces de teclado e comeca a ler
  usb_host_transfer_alloc(64, 0, &d.ctrl);
  d.aberto = true;
  uint8_t teclados = 0;
  for (uint8_t i = 0; i < d.n_itf; i++) {
    Interface& itf = d.itf[i];
    if (!itf.teclado_boot) continue;   // v0.1: so teclados boot (midia: proxima versao)
    if (usb_host_interface_claim(s_cliente, d.hdl, itf.numero, 0) != ESP_OK) {
      log_printf("[USB] itf %u: nao foi possivel reivindicar\n", itf.numero);
      continue;
    }
    const uint16_t tam = itf.tam_pacote ? itf.tam_pacote : 8;
    if (usb_host_transfer_alloc(tam, 0, &itf.xfer) != ESP_OK) {
      usb_host_interface_release(s_cliente, d.hdl, itf.numero);
      continue;
    }
    itf.ativa = true;
    itf.xfer->device_handle    = d.hdl;
    itf.xfer->bEndpointAddress = itf.endpoint;
    itf.xfer->callback         = cb_relatorio;
    itf.xfer->context          = &itf;
    itf.xfer->num_bytes        = tam;   // multiplo do tamanho do pacote
    itf.em_voo = (usb_host_transfer_submit(itf.xfer) == ESP_OK);
    d.protocolo_pendente |= (1u << i);
    teclados++;
  }

  if (!teclados) {
    log_printf("[USB] %04X:%04X nao tem interface de teclado\n", d.vid, d.pid);
    desmontar();
    return;
  }
  d.leds_pendente = true;   // sincroniza Caps/Num Lock com o PC
  snprintf(s_resumo, sizeof(s_resumo), "%04X:%04X boot", d.vid, d.pid);
  s_conectado = true;
  log_printf("[USB] teclado %s montado\n", s_resumo);
}

/* =====================================================================
   TAREFAS
   ===================================================================== */
static void cb_cliente(const usb_host_client_event_msg_t* msg, void* arg) {
  (void)arg;
  switch (msg->event) {
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
      s_novo_endereco = msg->new_dev.address;
      break;
    case USB_HOST_CLIENT_EVENT_DEV_GONE:
      if (s_disp.aberto && msg->dev_gone.dev_hdl == s_disp.hdl) s_saiu = true;
      break;
    default:
      break;
  }
}

static void tarefa_daemon(void*) {
  for (;;) {
    uint32_t flags = 0;
    usb_host_lib_handle_events(portMAX_DELAY, &flags);
    if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) usb_host_device_free_all();
  }
}

static void tarefa_cliente(void*) {
  usb_host_client_config_t cfg = {};
  cfg.is_synchronous    = false;
  cfg.max_num_event_msg = 5;
  cfg.async.client_event_callback = cb_cliente;
  cfg.async.callback_arg          = nullptr;
  if (usb_host_client_register(&cfg, &s_cliente) != ESP_OK) {
    log_printf("[USB] falha ao registrar o cliente\n");
    vTaskDelete(nullptr);
    return;
  }

  for (;;) {
    usb_host_client_handle_events(s_cliente, pdMS_TO_TICKS(10));

    if (s_saiu) {
      s_saiu = false;
      desmontar();
    }
    if (s_novo_endereco) {
      const uint8_t e = s_novo_endereco;
      s_novo_endereco = 0;
      montar(e);
    }
    processar_pendencias();
  }
}

/* =====================================================================
   API
   ===================================================================== */
void usbteclado_iniciar(QueueHandle_t fila) {
  s_fila = fila;

  usb_host_config_t cfg = {};
  cfg.skip_phy_setup = false;
  cfg.intr_flags     = ESP_INTR_FLAG_LEVEL1;
  if (usb_host_install(&cfg) != ESP_OK) {
    log_printf("[USB] falha ao instalar o USB host\n");
    return;
  }
  xTaskCreatePinnedToCore(tarefa_daemon,  "usb_daemon",  4096, nullptr, 5, nullptr, 0);
  xTaskCreatePinnedToCore(tarefa_cliente, "usb_cliente", 6144, nullptr, 5, nullptr, 0);
  log_printf("[USB] host iniciado: aguardando teclado\n");
}

void usbteclado_definir_leds(uint8_t leds) {
  s_leds = leds;
  s_disp.leds_pendente = true;   // a tarefa do cliente envia quando puder
}

bool usbteclado_conectado() { return s_conectado; }

const char* usbteclado_resumo() { return s_resumo; }
