#pragma once
#include <stdint.h>

// Кнопки управления: джойстик на H1 (если запаян) и BOOT на DevKit.
// Опрос неблокирующий, вызывать из loop() не реже ~50 раз в секунду.
namespace input {

enum class Event : uint8_t { None, Up, Down, Left, Right, Enter, Boot };

void begin();

// Следующее событие нажатия (по фронту, с антидребезгом) или Event::None.
Event poll();

}  // namespace input
