#include "dash_screen.h"

#include "../can/signals.h"
#include "../can/twai_bus.h"
#include "display.h"

namespace ui {

static constexpr uint32_t kRefreshMs = 100;   // 10 Гц: цифры оборотов успевают читаться
static constexpr uint32_t kBlinkMs = 500;     // мигание плашки двери
static constexpr uint32_t kStaleKey = 0xFFFFFFFFu;

static constexpr uint16_t kColdColor = lgfx::color565(80, 150, 255);  // синий, читаемый на чёрном
static constexpr uint16_t kDimColor = lgfx::color565(70, 70, 70);

// Кириллица есть только в efont (bitmap, масштабируется целиком)
static const lgfx::IFont* const kCyrFont = &fonts::efontKR_24_b;

// Область экрана со своим спрайтом. key — что сейчас нарисовано.
struct Widget {
  int16_t x, y, w, h;
  LGFX_Sprite spr;
  uint32_t key;
  bool drawn;
};

// 480x320: сверху плашка двери, в середине ОЖ | обороты | селектор, внизу тормоз
static Widget s_door{0, 0, 480, 56, LGFX_Sprite(&tft), 0, false};
static Widget s_coolant{0, 64, 120, 172, LGFX_Sprite(&tft), 0, false};
static Widget s_rpm{120, 64, 260, 172, LGFX_Sprite(&tft), 0, false};
static Widget s_selector{380, 64, 100, 172, LGFX_Sprite(&tft), 0, false};
static Widget s_brake{120, 248, 260, 64, LGFX_Sprite(&tft), 0, false};
static Widget* const kWidgets[] = {&s_door, &s_coolant, &s_rpm, &s_selector, &s_brake};

// Подпись кириллицей: масштаб 1 или 2, чтобы влезла в maxW
static void cyrText(LGFX_Sprite& s, const char* text, int cx, int cy, int maxW, uint16_t color) {
  s.setFont(kCyrFont);
  s.setTextSize(1);
  s.setTextSize(s.textWidth(text) * 2 <= maxW ? 2 : 1);
  s.setTextDatum(textdatum_t::middle_center);
  s.setTextColor(color);
  s.drawString(text, cx, cy);
}

static void bigText(LGFX_Sprite& s, const lgfx::IFont* font, float size, const char* text, int cx,
                    int cy, uint16_t color) {
  s.setFont(font);
  s.setTextSize(size);
  s.setTextDatum(textdatum_t::middle_center);
  s.setTextColor(color);
  s.drawString(text, cx, cy);
}

// Перерисовать виджет, если показ изменился
template <typename Draw>
static void update(Widget& wd, uint32_t key, Draw draw) {
  if (wd.drawn && wd.key == key) return;
  wd.key = key;
  wd.drawn = true;
  wd.spr.fillSprite(TFT_BLACK);
  draw(wd.spr, key);
  wd.spr.pushSprite(wd.x, wd.y);
}

static void drawDoor(const sig::DashState& st, uint32_t now) {
  uint32_t key = !st.door_open.fresh(now) ? kStaleKey
                 : !st.door_open.v        ? 0
                                          : 1 + (now / kBlinkMs) % 2;
  update(s_door, key, [](LGFX_Sprite& s, uint32_t k) {
    if (k == kStaleKey) {
      cyrText(s, "ДВЕРЬ --", s.width() / 2, s.height() / 2, 200, kDimColor);
    } else if (k == 1) {  // открыта, фаза «горит»
      s.fillRoundRect(4, 2, s.width() - 8, s.height() - 4, 8, TFT_RED);
      cyrText(s, "ДВЕРЬ ОТКРЫТА", s.width() / 2, s.height() / 2, s.width() - 24, TFT_WHITE);
    }
    // k == 0 (закрыта) и k == 2 (фаза «погасла») — пусто
  });
}

static void drawCoolant(const sig::DashState& st, uint32_t now) {
  uint32_t key = st.coolant_c.fresh(now) ? (uint32_t)(st.coolant_c.v + 1000) : kStaleKey;
  update(s_coolant, key, [](LGFX_Sprite& s, uint32_t k) {
    const int cx = s.width() / 2;
    if (k == kStaleKey) {
      cyrText(s, "ОЖ °C", cx, 28, s.width() - 8, kDimColor);
      bigText(s, &fonts::DejaVu40, 1, "--", cx, 100, kDimColor);
      return;
    }
    int c = (int)k - 1000;
    uint16_t fg = c < sig::kCoolantColdC ? kColdColor : TFT_WHITE;
    if (c > sig::kCoolantHotC) {
      // Перегрев — вся область красная, текст белый
      s.fillRoundRect(2, 2, s.width() - 4, s.height() - 4, 8, TFT_RED);
      fg = TFT_WHITE;
    }
    cyrText(s, "ОЖ °C", cx, 28, s.width() - 8, fg);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", c);
    bigText(s, &fonts::DejaVu40, 1, buf, cx, 100, fg);
  });
}

static void drawRpm(const sig::DashState& st, uint32_t now) {
  uint32_t key = st.rpm.fresh(now) ? st.rpm.v : kStaleKey;
  update(s_rpm, key, [](LGFX_Sprite& s, uint32_t k) {
    const int cx = s.width() / 2;
    char buf[8];
    if (k == kStaleKey) snprintf(buf, sizeof(buf), "--");
    else snprintf(buf, sizeof(buf), "%lu", (unsigned long)k);
    bigText(s, &fonts::Font8, 1, buf, cx, 72, k == kStaleKey ? kDimColor : TFT_WHITE);
    cyrText(s, "об/мин", cx, 146, s.width() - 20, kDimColor);
  });
}

static void drawSelector(const sig::DashState& st, uint32_t now) {
  // Unknown при свежих кадрах — код не из таблицы (3/2/1 не проверены) или
  // с момента старта были только 0x08: показываем «?»
  uint32_t key = st.selector.fresh(now) ? (uint32_t)st.selector.v : kStaleKey;
  update(s_selector, key, [](LGFX_Sprite& s, uint32_t k) {
    const char* txt = "--";
    if (k != kStaleKey) {
      switch ((sig::Selector)k) {
        case sig::Selector::P: txt = "P"; break;
        case sig::Selector::R: txt = "R"; break;
        case sig::Selector::N: txt = "N"; break;
        case sig::Selector::D: txt = "D"; break;
        case sig::Selector::Unknown: txt = "?"; break;
      }
    }
    uint16_t fg = k == kStaleKey ? kDimColor : TFT_WHITE;
    s.drawRoundRect(4, 4, s.width() - 8, s.height() - 8, 8, fg);
    bigText(s, &fonts::DejaVu72, k == kStaleKey ? 0.7f : 1.5f, txt, s.width() / 2,
            s.height() / 2, fg);
  });
}

static void drawBrake(const sig::DashState& st, uint32_t now) {
  uint32_t key = !st.brake.fresh(now) ? kStaleKey : st.brake.v ? 1 : 0;
  update(s_brake, key, [](LGFX_Sprite& s, uint32_t k) {
    const int cx = s.width() / 2, cy = s.height() / 2;
    if (k == 1) {
      s.fillRoundRect(2, 2, s.width() - 4, s.height() - 4, 8, TFT_RED);
      cyrText(s, "ТОРМОЗ", cx, cy, s.width() - 20, TFT_WHITE);
    } else {
      s.drawRoundRect(2, 2, s.width() - 4, s.height() - 4, 8, kDimColor);
      cyrText(s, k == kStaleKey ? "ТОРМОЗ --" : "ТОРМОЗ", cx, cy, s.width() - 20, kDimColor);
    }
  });
}

void dashBegin() {
  for (Widget* wd : kWidgets) {
    wd->spr.setPsram(true);
    wd->spr.setColorDepth(16);
    wd->spr.createSprite(wd->w, wd->h);
  }
}

void dashEnter() {
  tft.fillScreen(TFT_BLACK);
  for (Widget* wd : kWidgets) wd->drawn = false;
}

void dashLoop() {
  static uint32_t lastDraw;
  uint32_t now = millis();
  // Первый проход после dashEnter() — сразу, дальше не чаще kRefreshMs
  if (s_rpm.drawn && now - lastDraw < kRefreshMs) return;
  lastDraw = now;

  sig::DashState st = canbus::dash();
  drawDoor(st, now);
  drawCoolant(st, now);
  drawRpm(st, now);
  drawSelector(st, now);
  drawBrake(st, now);
}

}  // namespace ui
