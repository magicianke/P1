#pragma once
#include <stddef.h>
#include <stdint.h>

namespace canbus {

constexpr size_t kMaxIds = 160;

// Последнее состояние одного CAN ID — для сниффера и поиска сигналов
struct IdEntry {
  uint32_t id;
  bool ext;
  uint8_t dlc;
  uint8_t data[8];
  uint32_t count;
  uint32_t last_ms;
  uint32_t period_ms;      // сглаженный период прихода
  uint32_t changed_ms[8];  // когда последний раз менялся каждый байт
};

struct Stats {
  bool running;
  uint32_t rx_total;
  uint32_t rx_per_sec;
  uint32_t rx_missed;       // переполнение очереди драйвера
  uint32_t bus_errors;
  uint32_t serial_dropped;  // кадры, не влезшие в Serial
  uint32_t ids_overflow;    // ID, не влезшие в таблицу
};

// Запуск TWAI на 500 кбит/с в режиме LISTEN_ONLY (в шину ничего не передаётся).
bool begin(bool dumpToSerial);

Stats stats();

// Копия таблицы ID, отсортированной по ID. Возвращает число записей.
size_t snapshot(IdEntry* out, size_t max);

}  // namespace canbus
