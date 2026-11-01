#include "tela.h"
#include "placa.h"

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

static Arduino_DataBus* s_bus = nullptr;
static Arduino_GFX*     s_gfx = nullptr;

static int16_t W = 320, H = 172;
static constexpr int16_t BARRA_H = 26;

void tela_iniciar() {
  s_bus = new Arduino_ESP32SPI(LCD_PIN_DC, LCD_PIN_CS, LCD_PIN_SCLK, LCD_PIN_MOSI, GFX_NOT_DEFINED);
  s_gfx = new Arduino_ST7789(s_bus, LCD_PIN_RST, LCD_ROTACAO, true /* IPS */,
                             LCD_LARGURA_NATIVA, LCD_ALTURA_NATIVA,
                             LCD_OFFSET_COLUNA, LCD_OFFSET_LINHA,
                             LCD_OFFSET_COLUNA, LCD_OFFSET_LINHA);
  s_gfx->begin();
  W = s_gfx->width();
  H = s_gfx->height();
  s_gfx->fillScreen(PRETO);

  ledcAttach(LCD_PIN_BL, 5000, 8);   // backlight por PWM (core ESP32 3.x)
  ledcWrite(LCD_PIN_BL, LCD_BRILHO);
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
