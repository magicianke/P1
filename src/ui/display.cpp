#include "display.h"

LGFX tft;

namespace ui {

static const char* controllerName() {
  return cfg::kTftController == cfg::TftController::ST7796 ? "ST7796" : "ILI9488";
}

// Тестовый экран для подбора настроек в board_config.h:
// полосы R/G/B/W/K — цвета и инверсия, рамка — сдвиг и поворот,
// метка "0,0" — где у картинки левый верхний угол.
static void drawTestScreen() {
  const int w = tft.width();
  const int h = tft.height();

  struct Stripe { uint16_t color; uint16_t text; const char* label; };
  static const Stripe kStripes[] = {
      {TFT_RED, TFT_WHITE, "R"},   {TFT_GREEN, TFT_BLACK, "G"}, {TFT_BLUE, TFT_WHITE, "B"},
      {TFT_WHITE, TFT_BLACK, "W"}, {TFT_BLACK, TFT_WHITE, "K"},
  };
  constexpr int kCount = sizeof(kStripes) / sizeof(kStripes[0]);
  for (int i = 0; i < kCount; i++) {
    int x0 = w * i / kCount;
    int x1 = w * (i + 1) / kCount;
    tft.fillRect(x0, 0, x1 - x0, h, kStripes[i].color);
    tft.setTextDatum(textdatum_t::bottom_center);
    tft.setTextSize(3);
    tft.setTextColor(kStripes[i].text);
    tft.drawString(kStripes[i].label, (x0 + x1) / 2, h - 12);
  }

  // Рамка 3 px по самому краю: все четыре стороны должны быть видны целиком
  for (int i = 0; i < 3; i++) tft.drawRect(i, i, w - 2 * i, h - 2 * i, TFT_YELLOW);

  tft.setTextDatum(textdatum_t::top_left);
  tft.setTextSize(2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("0,0", 6, 6);

  // Надпись в чёрной плашке по центру
  tft.fillRect(w / 2 - 150, h / 2 - 60, 300, 120, TFT_BLACK);
  tft.drawRect(w / 2 - 150, h / 2 - 60, 300, 120, TFT_WHITE);
  tft.setTextDatum(textdatum_t::middle_center);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(5);
  tft.drawString("P1 TEST", w / 2, h / 2 - 22);
  tft.setTextSize(3);
  tft.setTextColor(TFT_CYAN);
  tft.drawString(controllerName(), w / 2, h / 2 + 18);
  tft.setTextSize(1);
  tft.setTextColor(TFT_LIGHTGREY);
  char line[64];
  snprintf(line, sizeof(line), "inv:%d bgr:%d rot:%u %luMHz %dx%d", cfg::kTftInvert, cfg::kTftBgr,
           (unsigned)cfg::kTftRotation, (unsigned long)(cfg::kTftSpiHz / 1000000), w, h);
  tft.drawString(line, w / 2, h / 2 + 46);
  tft.setTextDatum(textdatum_t::top_left);
}

void displayBegin() {
  tft.init();
  tft.setRotation(cfg::kTftRotation);  // подобрать под установку в дефлектор
  tft.setBrightness(255);              // на старте 100 %
  tft.fillScreen(TFT_BLACK);

  if (cfg::kTftTestMs) {
    drawTestScreen();
    Serial.printf("# P1 display test: %s inv:%d bgr:%d rot:%u spi:%luHz %dx%d\n", controllerName(),
                  cfg::kTftInvert, cfg::kTftBgr, (unsigned)cfg::kTftRotation,
                  (unsigned long)cfg::kTftSpiHz, tft.width(), tft.height());
    vTaskDelay(pdMS_TO_TICKS(cfg::kTftTestMs));
    tft.fillScreen(TFT_BLACK);
  }
}

}  // namespace ui
