#pragma once
#include "../input/buttons.h"

// Экран сниффера: таблица CAN ID с подсветкой изменившихся байтов.
// Листание: джойстик RIGHT/LEFT (BOOT переключает экраны, см. P1.ino).
namespace ui {
void snifferBegin();  // один раз: создать спрайт строки
void snifferEnter();  // при переключении на экран
void snifferLoop(input::Event ev);
}  // namespace ui
