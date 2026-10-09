#pragma once

// Экран сниффера: таблица CAN ID с подсветкой изменившихся байтов.
// Листание: джойстик RIGHT/LEFT, без H1 — кнопка BOOT (вперёд по кругу).
namespace ui {
void snifferBegin();
void snifferLoop();
}  // namespace ui
