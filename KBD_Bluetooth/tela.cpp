#include "tela.h"
#include "placa.h"
#include "log.h"

#include <Arduino_GFX_Library.h>

// Cores RGB565
static constexpr uint16_t PRETO    = 0x0000;
static constexpr uint16_t BRANCO   = 0xFFFF;
static constexpr uint16_t CINZA    = 0x7BEF;
static constexpr uint16_t CINZA_E  = 0x2104;
static constexpr uint16_t VERDE    = 0x07E0;
static constexpr uint16_t AZUL     = 0x3D7F;
static constexpr uint16_t AMARELO  = 0xFFE0;
static constexpr uint16_t LARANJA  = 0xFD20;
static constexpr uint16_t VERMELHO = 0xF800;

/* =====================================================================
   CONTROLADOR JD9853
   O JD9853 entende os comandos basicos do ST7789 (janela, rotacao,
   inversao), mas precisa da sequencia de inicializacao do fabricante
   (registradores de tensao, gama etc.). Os valores sao os da
   inicializacao de referencia da Waveshare para este painel.
   ===================================================================== */
class Arduino_JD9853 : public Arduino_ST7789 {
 public:
  using Arduino_ST7789::Arduino_ST7789;

  bool begin(int32_t speed = GFX_NOT_DEFINED) override {
    return Arduino_TFT::begin(speed);   // SPI modo 0 (o ST7789 da biblioteca usa outro)
  }

 protected:
  void tftInit() override {
    if (_rst != GFX_NOT_DEFINED) {
      pinMode(_rst, OUTPUT);
      digitalWrite(_rst, HIGH);
      delay(100);
      digitalWrite(_rst, LOW);
      delay(120);
      digitalWrite(_rst, HIGH);
      delay(120);
    }

    static const uint8_t init[] = {
      BEGIN_WRITE, WRITE_COMMAND_8, 0x11, END_WRITE,   // sair do modo sleep
      DELAY, 120,

      BEGIN_WRITE,
      WRITE_C8_D16, 0xDF, 0x98, 0x53,                   // libera os comandos do fabricante
      WRITE_C8_D8, 0xB2, 0x23,
      WRITE_COMMAND_8, 0xB7, WRITE_BYTES, 4, 0x00, 0x47, 0x00, 0x6F,
      WRITE_COMMAND_8, 0xBB, WRITE_BYTES, 6, 0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0,
      WRITE_C8_D16, 0xC0, 0x44, 0xA4,
      WRITE_C8_D8, 0xC1, 0x16,
      WRITE_COMMAND_8, 0xC3, WRITE_BYTES, 8, 0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77,
      WRITE_COMMAND_8, 0xC4, WRITE_BYTES, 12,
        0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A, 0x16, 0x79, 0x0B, 0x0A, 0x16, 0x82,
      WRITE_COMMAND_8, 0xC8, WRITE_BYTES, 32,                                   // gama
        0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
        0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
      WRITE_COMMAND_8, 0xD0, WRITE_BYTES, 5, 0x04, 0x06, 0x6B, 0x0F, 0x00,
      WRITE_C8_D16, 0xD7, 0x00, 0x30,
      WRITE_C8_D8, 0xE6, 0x14,
      WRITE_C8_D8, 0xDE, 0x01,
      WRITE_COMMAND_8, 0xB7, WRITE_BYTES, 5, 0x03, 0x13, 0xEF, 0x35, 0x35,
      WRITE_COMMAND_8, 0xC1, WRITE_BYTES, 3, 0x14, 0x15, 0xC0,
      WRITE_C8_D16, 0xC2, 0x06, 0x3A,
      WRITE_C8_D16, 0xC4, 0x72, 0x12,
      WRITE_C8_D8, 0xBE, 0x00,
      WRITE_C8_D8, 0xDE, 0x02,
      WRITE_COMMAND_8, 0xE5, WRITE_BYTES, 3, 0x00, 0x02, 0x00,
      WRITE_COMMAND_8, 0xE5, WRITE_BYTES, 3, 0x01, 0x02, 0x00,
      WRITE_C8_D8, 0xDE, 0x00,
      WRITE_C8_D8, 0x35, 0x00,                          // sinal de tearing
      WRITE_C8_D8, 0x3A, 0x05,                          // 16 bits por pixel (RGB565)
      WRITE_COMMAND_8, 0x2A, WRITE_BYTES, 4, 0x00, 0x22, 0x00, 0xCD,   // colunas 34..205
      WRITE_COMMAND_8, 0x2B, WRITE_BYTES, 4, 0x00, 0x00, 0x01, 0x3F,   // linhas 0..319
      WRITE_C8_D8, 0xDE, 0x02,
      WRITE_COMMAND_8, 0xE5, WRITE_BYTES, 3, 0x00, 0x02, 0x00,
      WRITE_C8_D8, 0xDE, 0x00,
      WRITE_C8_D8, 0x36, 0x00,
      WRITE_COMMAND_8, 0x21,                            // cores invertidas (painel IPS)
      END_WRITE,
      DELAY, 10,

      BEGIN_WRITE, WRITE_COMMAND_8, 0x29, END_WRITE,   // liga a tela
    };
    _bus->batchOperation(init, sizeof(init));
    invertDisplay(false);   // com ips = true, mantem a inversao ligada
  }
};

static Arduino_DataBus* s_bus = nullptr;
static Arduino_GFX*     s_gfx = nullptr;

static int16_t W = 320, H = 172;
static constexpr int16_t BARRA_H = 26;

bool tela_iniciar() {
  // Backlight apagado ate a primeira imagem estar pronta
  pinMode(LCD_PIN_BL, OUTPUT);
  digitalWrite(LCD_PIN_BL, LOW);

  if (LCD_BARRAMENTO_HWSPI) {
    // Usa a biblioteca SPI do proprio core: funciona em qualquer versao do core
    s_bus = new Arduino_HWSPI(LCD_PIN_DC, LCD_PIN_CS, LCD_PIN_SCLK, LCD_PIN_MOSI, GFX_NOT_DEFINED);
  } else {
    // Acesso direto aos registradores do SPI (mais rapido, mais sensivel a versao do core)
    s_bus = new Arduino_ESP32SPI(LCD_PIN_DC, LCD_PIN_CS, LCD_PIN_SCLK, LCD_PIN_MOSI, GFX_NOT_DEFINED, FSPI);
  }
  if (LCD_INIT_JD9853) {
    s_gfx = new Arduino_JD9853(s_bus, LCD_PIN_RST, LCD_ROTACAO, true /* IPS */,
                               LCD_LARGURA_NATIVA, LCD_ALTURA_NATIVA,
                               LCD_OFFSET_COLUNA, LCD_OFFSET_LINHA,
                               LCD_OFFSET_COLUNA, LCD_OFFSET_LINHA);
  } else {
    s_gfx = new Arduino_ST7789(s_bus, LCD_PIN_RST, LCD_ROTACAO, true /* IPS */,
                               LCD_LARGURA_NATIVA, LCD_ALTURA_NATIVA,
                               LCD_OFFSET_COLUNA, LCD_OFFSET_LINHA,
                               LCD_OFFSET_COLUNA, LCD_OFFSET_LINHA);
  }
  const bool ok = s_gfx->begin(LCD_SPI_HZ);
  kbd_log("[TELA] begin %s (%s, %s, %lu Hz)\n", ok ? "ok" : "FALHOU",
          LCD_INIT_JD9853 ? "JD9853" : "ST7789",
          LCD_BARRAMENTO_HWSPI ? "HWSPI" : "ESP32SPI", (unsigned long)LCD_SPI_HZ);
  W = s_gfx->width();
  H = s_gfx->height();

  // Teste de cores: vermelho, verde, azul. Se aparecerem, o LCD esta ok.
  if (LCD_TESTE_CORES) {
    s_gfx->fillScreen(VERMELHO);
    analogWrite(LCD_PIN_BL, LCD_BRILHO);
    delay(400);
    s_gfx->fillScreen(VERDE);
    delay(400);
    s_gfx->fillScreen(0x001F);   // azul puro
    delay(400);
  }
  s_gfx->fillScreen(PRETO);
  analogWrite(LCD_PIN_BL, LCD_BRILHO);   // backlight por PWM
  return ok;
}

// Texto centralizado numa faixa, apagando o fundo da faixa antes
static void texto_centro(const char* s, int16_t y, uint8_t tam, uint16_t cor, int16_t x0 = 0, int16_t w = -1) {
  if (w < 0) w = W;
  const int16_t larg = (int16_t)strlen(s) * 6 * tam;
  s_gfx->fillRect(x0, y, w, 8 * tam, PRETO);
  s_gfx->setTextSize(tam);
  s_gfx->setTextColor(cor);
  s_gfx->setCursor(x0 + (w - larg) / 2, y);
  s_gfx->print(s);
}

void tela_mensagem(const char* texto, uint16_t cor) {
  if (!s_gfx) return;
  s_gfx->fillScreen(PRETO);
  texto_centro(texto, H / 2 - 12, 3, cor);
}

// Numero com separador de milhar: 1234567 -> "1.234.567"
static void milhar(char* out, size_t n, uint32_t v) {
  char tmp[16];
  snprintf(tmp, sizeof(tmp), "%lu", (unsigned long)v);
  const int len = (int)strlen(tmp);
  size_t o = 0;
  for (int i = 0; i < len && o + 2 < n; i++) {
    if (i && (len - i) % 3 == 0) out[o++] = '.';
    out[o++] = tmp[i];
  }
  out[o] = '\0';
}

static void desenhar_barra(const EstadoTela& st) {
  s_gfx->fillRect(0, 0, W, BARRA_H, CINZA_E);
  s_gfx->setTextSize(2);

  // Bluetooth
  const char* bt = st.ble == BLE_PRONTO ? "BT OK" : st.ble == BLE_CONECTADO ? "BT ..." : "BT parear";
  const uint16_t cor_bt = st.ble == BLE_PRONTO ? AZUL : st.ble == BLE_CONECTADO ? AMARELO : CINZA;
  s_gfx->setTextColor(cor_bt);
  s_gfx->setCursor(6, 5);
  s_gfx->print(bt);

  // Teclado USB
  s_gfx->setTextColor(st.usb_ok ? VERDE : VERMELHO);
  s_gfx->setCursor(130, 5);
  s_gfx->print(st.usb_ok ? "USB OK" : "USB --");

  // Caps / Num Lock
  s_gfx->setTextColor((st.leds & 0x02) ? AMARELO : CINZA);
  s_gfx->setCursor(W - 60, 5);
  s_gfx->print("A");
  s_gfx->setTextColor((st.leds & 0x01) ? AMARELO : CINZA);
  s_gfx->setCursor(W - 30, 5);
  s_gfx->print("1");
}

void tela_atualizar(const EstadoTela& st, const Estatisticas& e, bool forcar) {
  if (!s_gfx) return;
  static EstadoTela   ult_st = {};
  static Estatisticas ult_e  = {};
  static bool         primeira = true;

  if (forcar || primeira) s_gfx->fillScreen(PRETO);

  if (forcar || primeira || st.ble != ult_st.ble || st.usb_ok != ult_st.usb_ok || st.leds != ult_st.leds)
    desenhar_barra(st);

  char buf[32];
  if (forcar || primeira || e.sessao != ult_e.sessao) {
    milhar(buf, sizeof(buf), e.sessao);
    texto_centro(buf, 46, 5, BRANCO);
    texto_centro("teclas nesta sessao", 92, 1, CINZA);
  }

  if (forcar || primeira || e.total != ult_e.total) {
    char n[20];
    milhar(n, sizeof(n), e.total);
    snprintf(buf, sizeof(buf), "Total %s", n);
    texto_centro(buf, 118, 2, VERDE);
  }

  if (forcar || primeira || e.wpm != ult_e.wpm || e.backspaces != ult_e.backspaces) {
    const uint32_t pct10 = e.sessao ? (uint32_t)(((uint64_t)e.backspaces * 1000ULL) / e.sessao) : 0;
    snprintf(buf, sizeof(buf), "WPM %u", e.wpm);
    texto_centro(buf, 146, 2, e.wpm >= 60 ? AMARELO : BRANCO, 0, W / 2);
    snprintf(buf, sizeof(buf), "Corr %lu.%lu%%", (unsigned long)(pct10 / 10), (unsigned long)(pct10 % 10));
    texto_centro(buf, 146, 2, pct10 >= 100 ? LARANJA : BRANCO, W / 2, W / 2);
  }

  ult_st = st;
  ult_e = e;
  primeira = false;
}
