#include "sniffer_screen.h"

#include "../can/twai_bus.h"
#include "../input/buttons.h"
#include "display.h"

namespace ui {

static constexpr int kHeaderH = 40;
static constexpr int kRowH = 18;
static constexpr uint32_t kRefreshMs = 200;
static constexpr uint32_t kChangedHighlightMs = 1000;
static constexpr uint32_t kStaleMs = 2000;

static LGFX_Sprite s_row(&tft);
static canbus::IdEntry s_entries[canbus::kMaxIds];
static int s_page;

static int rowsPerPage() { return (tft.height() - kHeaderH) / kRowH; }

static void drawHeader(const canbus::Stats& st, size_t ids, int pages) {
  s_row.fillSprite(TFT_NAVY);
  s_row.setTextColor(st.running ? TFT_GREEN : TFT_RED);
  s_row.setCursor(4, 1);
  s_row.printf("500k LISTEN %4lu fr/s IDs:%u", (unsigned long)st.rx_per_sec, (unsigned)ids);
  s_row.setTextColor(TFT_LIGHTGREY);
  s_row.setCursor(s_row.width() - 12 * 5 - 4, 1);
  s_row.printf("%d/%d", s_page + 1, pages);
  s_row.pushSprite(0, 0);

  s_row.fillSprite(TFT_BLACK);
  s_row.setTextColor(st.rx_missed || st.bus_errors || st.serial_dropped ? TFT_ORANGE : TFT_DARKGREY);
  s_row.setCursor(4, 1);
  s_row.printf("miss:%lu err:%lu drop:%lu", (unsigned long)st.rx_missed,
               (unsigned long)st.bus_errors, (unsigned long)st.serial_dropped);
  s_row.pushSprite(0, kRowH + 2);
}

static void drawEntry(const canbus::IdEntry& e, uint32_t now) {
  bool stale = now - e.last_ms > kStaleMs;
  s_row.fillSprite(TFT_BLACK);
  s_row.setCursor(4, 1);
  s_row.setTextColor(stale ? TFT_DARKGREY : TFT_CYAN);
  if (e.ext) s_row.printf("%08lX", (unsigned long)e.id);
  else s_row.printf("%03lX", (unsigned long)e.id);
  s_row.setTextColor(TFT_DARKGREY);
  s_row.printf(" %u", e.dlc);
  for (uint8_t i = 0; i < e.dlc; i++) {
    bool changed = e.changed_ms[i] && now - e.changed_ms[i] < kChangedHighlightMs;
    s_row.setTextColor(stale ? TFT_DARKGREY : changed ? TFT_YELLOW : TFT_WHITE);
    s_row.printf(" %02X", e.data[i]);
  }
  s_row.setTextColor(TFT_DARKGREY);
  s_row.setCursor(s_row.width() - 12 * 4 - 4, 1);
  s_row.printf("%4lu", (unsigned long)(e.period_ms > 9999 ? 9999 : e.period_ms));
}

void snifferBegin() {
  s_row.setColorDepth(16);
  s_row.createSprite(tft.width(), kRowH);
  s_row.setFont(&fonts::Font0);
  s_row.setTextSize(2);
  tft.fillScreen(TFT_BLACK);
}

void snifferLoop() {
  static uint32_t lastDraw;
  uint32_t now = millis();

  // RIGHT или BOOT — следующая страница, LEFT — предыдущая
  input::Event ev = input::poll();
  int step = ev == input::Event::Right || ev == input::Event::Boot ? 1
             : ev == input::Event::Left                            ? -1
                                                                   : 0;

  if (step == 0 && now - lastDraw < kRefreshMs) return;
  lastDraw = now;

  size_t n = canbus::snapshot(s_entries, canbus::kMaxIds);
  int perPage = rowsPerPage();
  int pages = n == 0 ? 1 : (int)((n + perPage - 1) / perPage);
  s_page = (s_page + step + pages) % pages;

  drawHeader(canbus::stats(), n, pages);

  int y = kHeaderH;
  for (int r = 0; r < perPage; r++, y += kRowH) {
    size_t idx = (size_t)s_page * perPage + r;
    if (idx < n) drawEntry(s_entries[idx], now);
    else s_row.fillSprite(TFT_BLACK);
    s_row.pushSprite(0, y);
  }
}

}  // namespace ui
