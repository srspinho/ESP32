#ifndef LOG_H
#define LOG_H

#include <Arduino.h>

/* =====================================================================
   LOG NO SERIAL (UART0: GPIO43 TX / GPIO44 RX, 115200)

   A USB da placa esta ocupada com o teclado, entao o log sai pela UART.
   Pode ser chamado de qualquer tarefa (USB, Bluetooth ou loop): um mutex
   evita mensagens misturadas, e, se a fila de envio estiver cheia, a
   mensagem e descartada em vez de travar quem chamou.
   ===================================================================== */
void log_iniciar();
void log_printf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

#endif  // LOG_H
