#ifndef TELA_H
#define TELA_H

#include <Arduino.h>
#include "ble_teclado.h"
#include "estatisticas.h"

/* =====================================================================
   TELA (LCD ST7789 172x320, deitado: 320 x 172) - biblioteca Arduino_GFX

   Barra de cima: Bluetooth, teclado USB e Caps/Num Lock.
   Centro: odometro de teclas da sessao.
   Embaixo: total desde sempre, WPM e taxa de correcao.

   So redesenha o que mudou (sem piscar e sem gastar CPU a toa).
   ===================================================================== */

struct EstadoTela {
  EstadoBle   ble;
  bool        usb_ok;
  const char* usb_resumo;
  uint8_t     leds;          // bits HID: 0 = Num Lock, 1 = Caps Lock
  const char* usb_diag;      // ultimo evento do USB (linha de diagnostico)
  uint32_t    usb_relatorios;
};

bool tela_iniciar();   // false = o LCD nao respondeu ao begin()
void tela_mensagem(const char* texto, uint16_t cor);   // aviso em tela cheia
void tela_atualizar(const EstadoTela& st, const Estatisticas& e, bool forcar);

#endif  // TELA_H
