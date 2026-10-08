#pragma once

// Экран сниффера: таблица CAN ID с подсветкой изменившихся байтов.
// Тап по экрану — следующая страница.
namespace ui {
void snifferBegin();
void snifferLoop();
}  // namespace ui
