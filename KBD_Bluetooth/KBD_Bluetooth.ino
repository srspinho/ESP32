/* =====================================================================
   KBD_Bluetooth  v0.2.1
   Transforma um teclado USB com fio em teclado Bluetooth, contando as
   teclas no caminho.

   Placa: Waveshare ESP32-S3-Touch-LCD-1.47 (USB-C, LCD JD9853 172x320)

   [Teclado USB] --USB host--> [ESP32-S3] --Bluetooth LE (HID)--> [PC]
                                   |
                         contagem, tela, LED RGB

   Fluxo:
     - tarefas do USB (core 0): leem o teclado e poem cada relatorio
       numa fila (usb_teclado.cpp);
     - loop() (core 1): tira da fila, conta (estatisticas.cpp), manda ao
       PC pelo Bluetooth (ble_teclado.cpp) e atualiza a tela (tela.cpp);
     - LEDs Caps/Num Lock: o PC avisa pelo Bluetooth, e o loop repassa
       ao teclado pelo USB.

   Bibliotecas (Gerenciador de Bibliotecas da IDE):
     - NimBLE-Arduino 2.x (h2zero)
     - GFX Library for Arduino (Arduino_GFX, moononournation)
   Core: esp32 da Espressif 3.x. Configuracao da placa no README.md.
   ===================================================================== */

#include "placa.h"
#include "log.h"
#include "usb_teclado.h"
#include "ble_teclado.h"
#include "estatisticas.h"
#include "tela.h"

static QueueHandle_t s_fila_usb = nullptr;
static uint8_t       s_leds = 0;   // Caps/Num Lock vindos do PC

// LED RGB (WS2812). Esta placa nao tem: com LED_RGB_PIN = -1 nao faz nada.
static void led_rgb(uint8_t r, uint8_t g, uint8_t b) {
  if (LED_RGB_PIN >= 0) rgbLedWrite(LED_RGB_PIN, r, g, b);
}

/* ---------- LED RGB de status ----------
   vermelho fixo  : sem teclado no USB
   azul piscando  : esperando o PC conectar/parear
   amarelo        : conectado, aguardando o PC terminar o pareamento
   verde          : tudo pronto (apaga um instante a cada tecla) */
static void led_status(bool tecla) {
  static uint32_t piscar_ms = 0, tecla_ms = 0;
  static bool     aceso = false;
  const uint32_t agora = millis();
  const uint8_t  b = LED_RGB_BRILHO;
  if (tecla) tecla_ms = agora;

  uint8_t r = 0, g = 0, bl = 0;
  if (!usbteclado_conectado()) {
    r = b;
  } else {
    switch (bleteclado_estado()) {
      case BLE_ANUNCIANDO:
        if (agora - piscar_ms >= 500) { piscar_ms = agora; aceso = !aceso; }
        if (aceso) bl = b;
        break;
      case BLE_CONECTADO: r = b; g = b; break;
      case BLE_PRONTO:    if (agora - tecla_ms > 40) g = b; break;
    }
  }

  static uint32_t ultima_cor = 0xFFFFFFFF;
  const uint32_t cor = ((uint32_t)r << 16) | ((uint32_t)g << 8) | bl;
  if (cor != ultima_cor) {
    ultima_cor = cor;
    led_rgb(r, g, bl);
  }
}

/* ---------- Botao BOOT: segurar 3 s apaga os pareamentos ---------- */
static void botao_boot() {
  static uint32_t apertado_ms = 0;
  static bool     feito = false;
  if (digitalRead(BOTAO_BOOT_PIN) == LOW) {
    if (!apertado_ms) apertado_ms = millis() ? millis() : 1;
    if (!feito && millis() - apertado_ms >= BOTAO_APAGAR_PARES_MS) {
      feito = true;
      bleteclado_apagar_pareamentos();
      tela_mensagem("PAREAR DE NOVO", 0xFFE0 /* amarelo */);
      delay(1500);
      const EstadoTela st = { bleteclado_estado(), usbteclado_conectado(), usbteclado_resumo(), s_leds,
                            usbteclado_diagnostico(), usbteclado_relatorios() };
      tela_atualizar(st, estat_atual(), true);
    }
  } else {
    apertado_ms = 0;
    feito = false;
  }
}

void setup() {
  kbd_log_iniciar();
  kbd_log("\n=== KBD_Bluetooth v0.2.1 ===\n");
  pinMode(BOTAO_BOOT_PIN, INPUT_PULLUP);
  led_rgb(0, 0, 0);

  if (!tela_iniciar()) {
    // LCD nao respondeu: pisca roxo 5 vezes, se houver LED (o resto continua funcionando)
    for (int i = 0; i < 5; i++) {
      led_rgb(LED_RGB_BRILHO, 0, LED_RGB_BRILHO);
      delay(200);
      led_rgb(0, 0, 0);
      delay(200);
    }
  }
  tela_mensagem("KBD Bluetooth", 0xFFFF);

  estat_iniciar();
  bleteclado_iniciar();

  s_fila_usb = xQueueCreate(32, sizeof(RelatorioTeclado));
  usbteclado_iniciar(s_fila_usb);

  delay(800);
  const EstadoTela st = { bleteclado_estado(), usbteclado_conectado(), usbteclado_resumo(), s_leds,
                            usbteclado_diagnostico(), usbteclado_relatorios() };
  tela_atualizar(st, estat_atual(), true);
}

void loop() {
  // 1) Teclas: espera ate 5 ms por um relatorio (baixa latencia, sem girar a CPU)
  bool tecla = false;
  RelatorioTeclado r;
  if (xQueueReceive(s_fila_usb, &r, pdMS_TO_TICKS(5)) == pdTRUE) {
    do {
      estat_registrar(r.dados);
      bleteclado_enviar_teclado(r.dados);
      tecla = true;
    } while (xQueueReceive(s_fila_usb, &r, 0) == pdTRUE);
  }

  // 2) LEDs do PC -> teclado
  uint8_t leds;
  if (bleteclado_leds_mudaram(&leds)) {
    s_leds = leds;
    usbteclado_definir_leds(leds);
  }

  // 3) Contagem, tela, LED e botao
  estat_tick();
  static uint32_t tela_ms = 0;
  if (millis() - tela_ms >= 100) {   // no maximo 10 quadros por segundo
    tela_ms = millis();
    const EstadoTela st = { bleteclado_estado(), usbteclado_conectado(), usbteclado_resumo(), s_leds,
                            usbteclado_diagnostico(), usbteclado_relatorios() };
    tela_atualizar(st, estat_atual(), false);
  }
  led_status(tecla);
  botao_boot();
}
