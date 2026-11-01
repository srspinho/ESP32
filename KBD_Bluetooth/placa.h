#ifndef PLACA_H
#define PLACA_H

#include <Arduino.h>

/* =====================================================================
   PLACA: Waveshare ESP32-S3-Touch-LCD-1.47
   https://www.waveshare.com/esp32-s3-touch-lcd-1.47.htm
   https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.47

   LCD de 1.47" (172 x 320) com controlador JD9853, touch capacitivo
   AXS5106L, microSD por SPI e USB-C ligado a USB nativa do ESP32-S3.
   Esta placa NAO tem LED RGB.

   Todos os pinos fixos da placa ficam aqui. Se algum detalhe da sua
   revisao for diferente (offset de coluna, rotacao...), e so neste
   arquivo que se mexe.
   ===================================================================== */

/* ---------- LCD JD9853, 172 x 320, SPI ---------- */
constexpr int8_t LCD_PIN_MOSI = 39;
constexpr int8_t LCD_PIN_SCLK = 38;
constexpr int8_t LCD_PIN_CS   = 21;
constexpr int8_t LCD_PIN_DC   = 45;
constexpr int8_t LCD_PIN_RST  = 40;
constexpr int8_t LCD_PIN_BL   = 46;

constexpr int16_t LCD_LARGURA_NATIVA = 172;
constexpr int16_t LCD_ALTURA_NATIVA  = 320;
// A RAM do controlador tem 240 colunas; o vidro de 172 comeca na coluna 34.
// Se aparecer uma faixa de lixo na lateral, e este numero.
constexpr int16_t LCD_OFFSET_COLUNA  = 34;
constexpr int16_t LCD_OFFSET_LINHA   = 0;
constexpr uint8_t LCD_ROTACAO        = 1;     // 1 ou 3 = deitado (320 x 172)

// Inicializacao do controlador:
//   true : sequencia do fabricante para o JD9853 (o desta placa)
//   false: sequencia generica do ST7789 (o JD9853 aceita a maior parte dela)
constexpr bool     LCD_INIT_JD9853      = true;

// Barramento SPI do LCD:
//   true : Arduino_HWSPI (usa a biblioteca SPI do core; funciona no core 3.x)
//   false: Arduino_ESP32SPI (registradores diretos; mais rapido, mas
//          sensivel a versao do core e da Arduino_GFX)
constexpr bool     LCD_BARRAMENTO_HWSPI = true;
constexpr uint32_t LCD_SPI_HZ           = 40000000UL;  // se a imagem vier com lixo: 20000000
// Ao ligar, pinta a tela de vermelho, verde e azul (diagnostico).
constexpr bool     LCD_TESTE_CORES      = true;
constexpr uint8_t  LCD_BRILHO           = 180;   // 0..255 (PWM do backlight)

/* ---------- Touch AXS5106L (I2C) - ainda nao usado ---------- */
constexpr int8_t  TOUCH_PIN_SDA = 42;
constexpr int8_t  TOUCH_PIN_SCL = 41;
constexpr int8_t  TOUCH_PIN_RST = 47;
constexpr int8_t  TOUCH_PIN_INT = 48;
constexpr uint8_t TOUCH_ENDERECO = 0x63;

/* ---------- microSD (SPI) - ainda nao usado ---------- */
constexpr int8_t SD_PIN_CS   = 14;
constexpr int8_t SD_PIN_MOSI = 15;
constexpr int8_t SD_PIN_SCLK = 16;
constexpr int8_t SD_PIN_MISO = 17;

/* ---------- LED RGB ----------
   Esta placa nao tem LED RGB: -1 desliga o LED de status. Se um dia
   ligar um WS2812 externo, ponha o GPIO aqui (nunca 38: e o SCLK do LCD). */
constexpr int8_t  LED_RGB_PIN    = -1;
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
