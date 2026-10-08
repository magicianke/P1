#include "display.h"

LGFX tft;

namespace ui {

void displayBegin() {
  tft.init();
  tft.setRotation(1);  // альбомная 480x320; подобрать под установку в дефлектор
  tft.setBrightness(200);
  tft.fillScreen(TFT_BLACK);
}

}  // namespace ui
