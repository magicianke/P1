#include "twai_bus.h"

#include <Arduino.h>
#include <driver/twai.h>
#include <freertos/semphr.h>
#include <string.h>

#include "../board_config.h"

namespace canbus {

static SemaphoreHandle_t s_lock;
static IdEntry s_table[kMaxIds];
static size_t s_count;
static Stats s_stats;
static bool s_dump;

// Таблица отсортирована по ID — бинарный поиск, вставка со сдвигом (редко)
static IdEntry* findOrInsert(uint32_t id, bool ext) {
  const uint32_t key = id | (ext ? 0x80000000u : 0);
  size_t lo = 0, hi = s_count;
  while (lo < hi) {
    size_t mid = (lo + hi) / 2;
    uint32_t k = s_table[mid].id | (s_table[mid].ext ? 0x80000000u : 0);
    if (k == key) return &s_table[mid];
    if (k < key) lo = mid + 1; else hi = mid;
  }
  if (s_count >= kMaxIds) {
    s_stats.ids_overflow++;
    return nullptr;
  }
  memmove(&s_table[lo + 1], &s_table[lo], (s_count - lo) * sizeof(IdEntry));
  s_count++;
  IdEntry* e = &s_table[lo];
  memset(e, 0, sizeof(*e));
  e->id = id;
  e->ext = ext;
  return e;
}

static void update(const twai_message_t& m, uint32_t now) {
  IdEntry* e = findOrInsert(m.identifier, m.extd);
  if (!e) return;
  if (e->count > 0) {
    uint32_t dt = now - e->last_ms;
    e->period_ms = e->period_ms ? (e->period_ms * 7 + dt) / 8 : dt;
  }
  uint8_t dlc = m.data_length_code > 8 ? 8 : m.data_length_code;
  for (uint8_t i = 0; i < dlc; i++) {
    if (e->count > 0 && e->data[i] != m.data[i]) e->changed_ms[i] = now;
    e->data[i] = m.data[i];
  }
  e->dlc = dlc;
  e->count++;
  e->last_ms = now;
}

// Формат candump -L: (сек.мкс) can0 123#DEADBEEF
static void dump(const twai_message_t& m) {
  char line[48];
  uint64_t us = esp_timer_get_time();
  int n = snprintf(line, sizeof(line), "(%lu.%06lu) can0 %0*lX#",
                   (unsigned long)(us / 1000000), (unsigned long)(us % 1000000),
                   m.extd ? 8 : 3, (unsigned long)m.identifier);
  static const char hex[] = "0123456789ABCDEF";
  for (uint8_t i = 0; i < m.data_length_code && i < 8; i++) {
    line[n++] = hex[m.data[i] >> 4];
    line[n++] = hex[m.data[i] & 0xF];
  }
  line[n++] = '\n';
  if (Serial.availableForWrite() >= n) {
    Serial.write(line, n);
  } else {
    s_stats.serial_dropped++;
  }
}

static void rxTask(void*) {
  uint32_t secStart = millis();
  uint32_t secCount = 0;
  for (;;) {
    twai_message_t m;
    if (twai_receive(&m, pdMS_TO_TICKS(100)) == ESP_OK && !m.rtr) {
      uint32_t now = millis();
      xSemaphoreTake(s_lock, portMAX_DELAY);
      update(m, now);
      s_stats.rx_total++;
      xSemaphoreGive(s_lock);
      secCount++;
      if (s_dump) dump(m);
    }

    uint32_t now = millis();
    if (now - secStart >= 1000) {
      twai_status_info_t st;
      bool ok = twai_get_status_info(&st) == ESP_OK;
      xSemaphoreTake(s_lock, portMAX_DELAY);
      s_stats.rx_per_sec = secCount;
      if (ok) {
        s_stats.rx_missed = st.rx_missed_count + st.rx_overrun_count;
        s_stats.bus_errors = st.bus_error_count;
      }
      xSemaphoreGive(s_lock);
      secCount = 0;
      secStart = now;
    }
  }
}

bool begin(bool dumpToSerial) {
  s_dump = dumpToSerial;
  s_lock = xSemaphoreCreateMutex();

  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
      (gpio_num_t)pins::kCanTx, (gpio_num_t)pins::kCanRx, TWAI_MODE_LISTEN_ONLY);
  g.rx_queue_len = 256;
  twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g, &t, &f) != ESP_OK) return false;
  if (twai_start() != ESP_OK) return false;

  s_stats.running = true;
  xTaskCreatePinnedToCore(rxTask, "can_rx", 4096, nullptr, 5, nullptr, 0);
  return true;
}

Stats stats() {
  xSemaphoreTake(s_lock, portMAX_DELAY);
  Stats s = s_stats;
  xSemaphoreGive(s_lock);
  return s;
}

size_t snapshot(IdEntry* out, size_t max) {
  xSemaphoreTake(s_lock, portMAX_DELAY);
  size_t n = s_count < max ? s_count : max;
  memcpy(out, s_table, n * sizeof(IdEntry));
  xSemaphoreGive(s_lock);
  return n;
}

}  // namespace canbus
