#ifndef ESTATISTICAS_H
#define ESTATISTICAS_H

#include <Arduino.h>

/* =====================================================================
   CONTAGEM DE TECLAS (mesma regra do KBD_AMOLED_164)

   - teclas: +1 a cada relatorio em que aparece pelo menos uma tecla
     nova (modificadores sozinhos nao contam);
   - backspaces: +1 a cada Backspace apertado (taxa de correcao);
   - por tecla: quantas vezes cada codigo HID foi apertado (inclui os
     modificadores, codigos 0xE0..0xE7). So a quantidade e guardada,
     nunca a ordem: nao da para reconstruir o que foi digitado.

   "Sessao" zera a cada vez que a placa liga. A contagem do DIA precisa
   de relogio, e vem na proxima versao (hora pelo Wi-Fi/NTP).
   ===================================================================== */

struct Estatisticas {
  uint32_t total;            // desde sempre (salvo na flash)
  uint32_t sessao;           // desde que a placa ligou
  uint32_t backspaces;       // da sessao
  uint16_t wpm;              // ultimos 60 s
};

void                estat_iniciar();
void                estat_registrar(const uint8_t rel[8]);   // relatorio boot
void                estat_tick();                            // chamar no loop
const Estatisticas& estat_atual();
uint32_t            estat_por_tecla(uint8_t codigo);
bool                estat_mudou();   // true uma vez a cada mudanca (para a tela)

#endif  // ESTATISTICAS_H
