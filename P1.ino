// CAN-ридер для Subaru Impreza GH 2.0 / 4EAT TZ1
// Этап 1: дисплей + сниффер CAN (только прослушивание)

#include "src/board_config.h"
#include "src/can/twai_bus.h"
#include "src/ui/display.h"
#include "src/ui/sniffer_screen.h"

void setup() {
  Serial.setTxBufferSize(16384);
  Serial.begin(2000000);

  ui::displayBegin();
  if (!canbus::begin(cfg::kSerialDump)) {
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.drawString("TWAI init failed", 10, 10);
    for (;;) vTaskDelay(portMAX_DELAY);
  }
  ui::snifferBegin();
}

void loop() {
  ui::snifferLoop();
  vTaskDelay(pdMS_TO_TICKS(10));
}
