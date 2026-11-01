#ifndef BLE_TECLADO_H
#define BLE_TECLADO_H

#include <Arduino.h>

/* =====================================================================
   BLUETOOTH LE: teclado HID para o PC (biblioteca NimBLE-Arduino 2.x)

   O mapa de relatorios ja inclui as teclas de midia (Report ID 2),
   mesmo que a v0.1 ainda nao as use: mudar o mapa depois obrigaria a
   desparear e parear de novo no PC.
   ===================================================================== */

enum EstadoBle : uint8_t {
  BLE_ANUNCIANDO = 0,   // esperando o PC conectar / parear
  BLE_CONECTADO,        // conectado, mas o PC ainda nao ligou as notificacoes
  BLE_PRONTO,           // conectado e recebendo teclas
};

void      bleteclado_iniciar();
EstadoBle bleteclado_estado();

// Relatorio boot de 8 bytes: [modificadores, 0, 6 teclas]
bool bleteclado_enviar_teclado(const uint8_t rel[8]);
// Uso da pagina Consumer (0xE9 = volume +, 0 = soltar)
bool bleteclado_enviar_midia(uint16_t uso);

// LEDs pedidos pelo PC (Caps/Num Lock). Retorna true uma vez a cada mudanca.
bool bleteclado_leds_mudaram(uint8_t* leds);

void bleteclado_apagar_pareamentos();
void bleteclado_definir_bateria(uint8_t porcento);

#endif  // BLE_TECLADO_H
