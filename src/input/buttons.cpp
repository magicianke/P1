#include "buttons.h"

#include <Arduino.h>

#include "../board_config.h"

namespace input {

static constexpr uint32_t kDebounceMs = 30;

struct Button {
  int pin;
  bool activeLow;
  Event event;
  bool stable;       // устоявшееся состояние: true — нажата
  bool raw;          // последнее прочитанное состояние
  uint32_t changed;  // когда менялось raw
};

static Button s_buttons[6];
static int s_count;

static void add(int pin, bool activeLow, Event ev) {
  if (pin < 0) return;
  pinMode(pin, activeLow ? INPUT_PULLUP : INPUT_PULLDOWN);
  s_buttons[s_count++] = {pin, activeLow, ev, false, false, 0};
}

void begin() {
  s_count = 0;
  // Энкодер (kEncA/kEncB) и потенциометр пока не используются.
  if (cfg::kControlsConnected) {
    add(pins::kJoyUp, cfg::kJoyActiveLow, Event::Up);
    add(pins::kJoyDown, cfg::kJoyActiveLow, Event::Down);
    add(pins::kJoyLeft, cfg::kJoyActiveLow, Event::Left);
    add(pins::kJoyRight, cfg::kJoyActiveLow, Event::Right);
    add(pins::kJoyEnter, cfg::kJoyActiveLow, Event::Enter);
  }
  add(pins::kBootButton, true, Event::Boot);
}

Event poll() {
  uint32_t now = millis();
  for (int i = 0; i < s_count; i++) {
    Button& b = s_buttons[i];
    bool pressed = (digitalRead(b.pin) == LOW) == b.activeLow;
    if (pressed != b.raw) {
      b.raw = pressed;
      b.changed = now;
    } else if (pressed != b.stable && now - b.changed >= kDebounceMs) {
      b.stable = pressed;
      if (pressed) return b.event;
    }
  }
  return Event::None;
}

}  // namespace input
