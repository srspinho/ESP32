#include "estatisticas.h"
#include "placa.h"
#include "log.h"

#include <Preferences.h>
#include <string.h>

static constexpr uint8_t HID_BACKSPACE = 0x2A;

static Preferences  s_prefs;
static Estatisticas s_e = {};
static uint32_t     s_por_tecla[256];
static bool         s_mudou = true;

// Estado do relatorio anterior (deteccao de tecla nova)
static uint32_t s_anterior[8] = { 0 };
static uint8_t  s_mods_anterior = 0;

// WPM: teclas em 12 fatias de 5 s (janela de 60 s)
static constexpr uint8_t  FATIAS = 12;
static constexpr uint32_t FATIA_MS = 5000;
static uint16_t s_fatias[FATIAS] = { 0 };
static uint8_t  s_fatia = 0;
static uint32_t s_fatia_ms = 0;

// Gravacao na flash
static bool     s_pendente = false;     // ha contagem nao salva
static uint32_t s_ultima_tecla_ms = 0;
static uint32_t s_ultimo_salvo_ms = 0;

static void salvar() {
  s_prefs.putUInt("total", s_e.total);
  s_prefs.putBytes("teclas", s_por_tecla, sizeof(s_por_tecla));
  s_pendente = false;
  s_ultimo_salvo_ms = millis();
  kbd_log("[ESTAT] salvo: total %lu\n", (unsigned long)s_e.total);
}

void estat_iniciar() {
  s_prefs.begin("kbd", false);
  s_e.total = s_prefs.getUInt("total", 0);
  memset(s_por_tecla, 0, sizeof(s_por_tecla));
  if (s_prefs.getBytesLength("teclas") == sizeof(s_por_tecla))
    s_prefs.getBytes("teclas", s_por_tecla, sizeof(s_por_tecla));
  s_fatia_ms = s_ultimo_salvo_ms = millis();
  kbd_log("[ESTAT] total carregado: %lu\n", (unsigned long)s_e.total);
}

void estat_registrar(const uint8_t rel[8]) {
  uint32_t atual[8] = { 0 };
  bool rollover = false;
  for (int i = 2; i < 8; i++) {
    const uint8_t k = rel[i];
    if (k >= 0x01 && k <= 0x03) rollover = true;   // ErrorRollOver: relatorio invalido
    else if (k) atual[k >> 5] |= 1UL << (k & 31);
  }
  if (rollover) return;

  bool nova = false;
  for (int w = 0; w < 8; w++) {
    uint32_t novas = atual[w] & ~s_anterior[w];
    if (!novas) continue;
    nova = true;
    while (novas) {
      const int b = __builtin_ctz(novas);
      novas &= novas - 1;
      s_por_tecla[w * 32 + b]++;
    }
  }
  // Modificadores apertados (0xE0..0xE7)
  uint8_t mods_novos = rel[0] & ~s_mods_anterior;
  while (mods_novos) {
    const int b = __builtin_ctz(mods_novos);
    mods_novos &= mods_novos - 1;
    s_por_tecla[0xE0 + b]++;
  }

  if (nova) {
    s_e.total++;
    s_e.sessao++;
    s_fatias[s_fatia]++;
    s_pendente = true;
    s_ultima_tecla_ms = millis();
    s_mudou = true;
  }
  if ((atual[HID_BACKSPACE >> 5] & ~s_anterior[HID_BACKSPACE >> 5]) & (1UL << (HID_BACKSPACE & 31))) {
    s_e.backspaces++;
  }

  memcpy(s_anterior, atual, sizeof(s_anterior));
  s_mods_anterior = rel[0];
}

void estat_tick() {
  const uint32_t agora = millis();

  // Avanca a janela do WPM
  while (agora - s_fatia_ms >= FATIA_MS) {
    s_fatia_ms += FATIA_MS;
    s_fatia = (uint8_t)((s_fatia + 1) % FATIAS);
    s_fatias[s_fatia] = 0;
    uint32_t soma = 0;
    for (uint16_t f : s_fatias) soma += f;
    const uint16_t wpm = (uint16_t)(soma / 5);   // 5 teclas = 1 palavra, janela de 1 min
    if (wpm != s_e.wpm) { s_e.wpm = wpm; s_mudou = true; }
  }

  // Grava periodicamente, ou logo que a digitacao para
  if (s_pendente &&
      (agora - s_ultimo_salvo_ms >= ESTAT_SALVAR_MS ||
       agora - s_ultima_tecla_ms >= ESTAT_PAUSA_SALVAR_MS)) {
    salvar();
  }
}

const Estatisticas& estat_atual() { return s_e; }

uint32_t estat_por_tecla(uint8_t codigo) { return s_por_tecla[codigo]; }

bool estat_mudou() {
  const bool m = s_mudou;
  s_mudou = false;
  return m;
}
