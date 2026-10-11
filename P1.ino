// CAN-ридер для Subaru Impreza GH 2.0 / 4EAT TZ1
// Этап 1: дисплей + сниффер CAN (только прослушивание)

#include "src/board_config.h"
#include "src/can/twai_bus.h"
#include "src/input/buttons.h"
#include "src/ui/display.h"
#include "src/ui/sniffer_screen.h"

// Дисплей только на запись (без MISO), поэтому init() не ждёт ответа
// и не зависает, даже если модуль не подключён.
void setup() {
  Serial.setTxBufferSize(16384);
  Serial.begin(2000000);

  ui::displayBegin();
  if (!canbus::begin(cfg::kSerialDump)) {
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.drawString("TWAI init failed", 10, 10);
    // USB-хост мог ещё не открыть порт — повторяем сообщение
    for (;;) {
      Serial.println("# P1 TWAI init failed");
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }
  input::begin();
  ui::snifferBegin();
}

// Строка-комментарий раз в kAliveMs: без кадров на шине видно, что прошивка жива.
// Строка пишется целиком одним write, чтобы не перемешаться с дампом из задачи can_rx.
static void serialHeartbeat() {
  static uint32_t last;
  uint32_t now = millis();
  if (now - last < cfg::kAliveMs) return;
  last = now;
  canbus::Stats st = canbus::stats();
  char line[96];
  int n = snprintf(line, sizeof(line), "# P1 alive %lus rx:%lu fr/s:%lu miss:%lu err:%lu drop:%lu\n",
                   (unsigned long)(now / 1000), (unsigned long)st.rx_total,
                   (unsigned long)st.rx_per_sec, (unsigned long)st.rx_missed,
                   (unsigned long)st.bus_errors, (unsigned long)st.serial_dropped);
  if (Serial.availableForWrite() >= n) Serial.write(line, n);
}

void loop() {
  ui::snifferLoop();
  serialHeartbeat();
  vTaskDelay(pdMS_TO_TICKS(10));
}
