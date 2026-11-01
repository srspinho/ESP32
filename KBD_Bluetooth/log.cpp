#include "log.h"
#include <stdarg.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static SemaphoreHandle_t s_mutex = nullptr;

void kbd_log_iniciar() {
  Serial.begin(115200);
  s_mutex = xSemaphoreCreateMutex();
}

void kbd_log(const char* fmt, ...) {
  char buf[192];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n <= 0 || !s_mutex) return;
  if (n > (int)sizeof(buf) - 1) n = sizeof(buf) - 1;

  if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(5)) != pdTRUE) return;
  if (Serial.availableForWrite() >= n) Serial.write((const uint8_t*)buf, n);
  xSemaphoreGive(s_mutex);
}
