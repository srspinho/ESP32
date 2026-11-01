#ifndef PLACA_H
#define PLACA_H

#include <Arduino.h>

/* =====================================================================
   PLACA: Waveshare ESP32-S3-LCD-1.47B (versao com USB-C femea)
   https://www.waveshare.com/wiki/ESP32-S3-LCD-1.47B

   Todos os pinos fixos da placa ficam aqui. Se algum detalhe da sua
   revisao for diferente (offset de coluna do LCD, pino do backlight...),
   e so neste arquivo que se mexe.

   Os pinos marcados "A CONFIRMAR" vieram da versao 1.47 (USB-A macho),
   porque a tabela da 1.47B nao lista MOSI/SCLK. Se a tela ficar preta
   com o backlight aceso, confira MOSI e SCLK no esquematico.
   ===================================================================== */

/* ---------- LCD ST7789, 172 x 320, SPI ---------- */
constexpr int8_t LCD_PIN_MOSI = 45;   // A CONFIRMAR na 1.47B
constexpr int8_t LCD_PIN_SCLK = 40;   // A CONFIRMAR na 1.47B
constexpr int8_t LCD_PIN_CS   = 42;
constexpr int8_t LCD_PIN_DC   = 41;
constexpr int8_t LCD_PIN_RST  = 39;
constexpr int8_t LCD_PIN_BL   = 46;   // 1.47B: GPIO46 (na 1.47 "pendrive" era GPIO48)

constexpr int16_t LCD_LARGURA_NATIVA = 172;
constexpr int16_t LCD_ALTURA_NATIVA  = 320;
// O ST7789 tem RAM de 240 colunas; o vidro de 172 comeca na coluna 34.
// Se aparecer uma faixa de lixo na lateral, e este numero.
constexpr int16_t LCD_OFFSET_COLUNA  = 34;
constexpr int16_t LCD_OFFSET_LINHA   = 0;
constexpr uint8_t LCD_ROTACAO        = 1;     // 1 = deitado (320 x 172)

// Barramento SPI do LCD:
//   true : Arduino_HWSPI (usa a biblioteca SPI do core; funciona no core 3.x)
//   false: Arduino_ESP32SPI (registradores diretos; mais rapido, mas
//          sensivel a versao do core e da Arduino_GFX)
constexpr bool     LCD_BARRAMENTO_HWSPI = true;
constexpr uint32_t LCD_SPI_HZ           = 40000000UL;  // se a imagem vier com lixo: 20000000
// Ao ligar, pinta a tela de vermelho, verde e azul (diagnostico).
constexpr bool     LCD_TESTE_CORES      = true;
constexpr uint8_t LCD_BRILHO         = 180;   // 0..255 (PWM do backlight)

/* ---------- LED RGB (WS2812) ---------- */
constexpr int8_t  LED_RGB_PIN    = 38;
constexpr uint8_t LED_RGB_BRILHO = 12;        // 0..255; o LED e muito forte

/* ---------- Botao BOOT ----------
   Segurar por 3 s com o firmware rodando: apaga os pareamentos
   Bluetooth (para parear com outro computador). */
constexpr int8_t   BOTAO_BOOT_PIN      = 0;
constexpr uint32_t BOTAO_APAGAR_PARES_MS = 3000;

/* ---------- USB Host ----------
   O USB-C da placa vai direto na USB nativa do ESP32-S3 (GPIO19/20).
   Com este firmware ele vira USB HOST: o teclado e ligado nele por um
   adaptador OTG. Os 5 V do teclado NAO saem do ESP32: veja a secao
   "Alimentacao" do README. */
constexpr uint8_t USB_MAX_INTERFACES = 4;     // interfaces HID por teclado

/* ---------- Bluetooth ---------- */
#define BLE_NOME_DISPOSITIVO "KBD Bluetooth"
#define BLE_FABRICANTE       "Pinheiro Tecnologia"

/* ---------- Estatisticas ----------
   Gravacao na flash (NVS): a cada ESTAT_SALVAR_MS se houve digitacao,
   e tambem quando a digitacao para por ESTAT_PAUSA_SALVAR_MS. Nunca a
   cada tecla, para nao gastar a flash. */
constexpr uint32_t ESTAT_SALVAR_MS       = 5UL * 60UL * 1000UL;
constexpr uint32_t ESTAT_PAUSA_SALVAR_MS = 30UL * 1000UL;

/* DEPURACAO (Serial = UART0, GPIO43 TX / GPIO44 RX, 115200)
   A USB da placa esta ocupada com o teclado, entao o log sai pela UART.
   true: imprime descritores e relatorios do teclado. */
constexpr bool USB_DEPURAR = false;

#endif  // PLACA_H
