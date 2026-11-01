#ifndef USB_TECLADO_H
#define USB_TECLADO_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

/* =====================================================================
   USB HOST: le o teclado fisico (driver usb_host do ESP-IDF)

   Duas tarefas no core 0:
     - "usb_daemon": eventos da biblioteca do host;
     - "usb_cliente": abre o teclado, le os relatorios e manda os
       pedidos de controle (protocolo boot, LEDs).

   Cada relatorio de teclado (formato boot, 8 bytes) vai para a fila
   passada em usbteclado_iniciar(). O loop() do Arduino consome a fila.

   Versao 0.1: teclados no formato "boot" (todos os teclados comuns),
   ligados direto, sem hub. Teclas de midia e teclados NKRO puros vem
   na proxima versao (parser de descritor do KBD_AMOLED_164).
   ===================================================================== */

struct RelatorioTeclado {
  uint8_t dados[8];   // [modificadores, reservado, 6 teclas]
};

void        usbteclado_iniciar(QueueHandle_t fila);
void        usbteclado_definir_leds(uint8_t leds);   // Caps/Num/Scroll Lock (bits HID)
bool        usbteclado_conectado();                  // ha um teclado montado
const char* usbteclado_resumo();                     // ex.: "046D:C31C boot"

#endif  // USB_TECLADO_H
