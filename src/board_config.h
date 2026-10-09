#pragma once
#include <stdint.h>

// Плата P1 ревизии 1.0. Источник истины — docs/hardware.md (и схема в docs/hardware/).
// Сначала меняется hardware.md, потом этот файл.
//
// Модуль: ESP32-S3-DevKitC-1 N16R8 (16 МБ flash, 8 МБ OPI PSRAM).
// НЕ использовать:
//   GPIO0, 3, 45, 46 — strapping-пины (GPIO0 — только чтение кнопки BOOT после старта)
//   GPIO19, 20       — нативный USB (D-/D+)
//   GPIO26–32        — SPI flash
//   GPIO33–37        — OPI PSRAM
//   GPIO43, 44       — UART0 (мост USB-UART)

namespace pins {

// Дисплей ILI9488, гнездо H5. SCK не на IOMUX-пине — сигнал идёт через GPIO matrix.
constexpr int kTftSclk = 10;  // H5 pin 7
constexpr int kTftMosi = 11;  // H5 pin 6 (SDI)
constexpr int kTftMiso = -1;  // SDO (GPIO46, strapping) не используется: контакт не пропаян
constexpr int kTftCs   = 14;  // H5 pin 3
constexpr int kTftDc   = 12;  // H5 pin 5
constexpr int kTftRst  = 13;  // H5 pin 4
constexpr int kTftBl   = 9;   // H5 pin 8, подсветка (ШИМ)

// CAN: SN65HVD230 U3 (D — TX, R — RX)
constexpr int kCanTx = 41;  // U3 pin 1
constexpr int kCanRx = 21;  // U3 pin 4

// Органы управления, гребёнка H1: джойстик ALPS RKJXT1F42001 + потенциометр WH148
constexpr int kJoyUp    = 5;   // H1 pin 1
constexpr int kJoyDown  = 6;   // H1 pin 2
constexpr int kJoyLeft  = 7;   // H1 pin 3
constexpr int kJoyRight = 15;  // H1 pin 4
constexpr int kJoyEnter = 16;  // H1 pin 5
constexpr int kEncA     = 17;  // H1 pin 6
constexpr int kEncB     = 8;   // H1 pin 7
constexpr int kPotBright = 4;  // H1 pin 8, ADC1_CH3 (работает вместе с Wi-Fi)

// Кнопка BOOT на DevKit (замыкает на GND, внешняя подтяжка на плате)
constexpr int kBootButton = 0;

// LED_DATA_3.3V на GPIO45 — strapping-пин, в ревизии 1.0 не использовать
// (см. «Известные проблемы» в docs/hardware.md).

}  // namespace pins

namespace cfg {

// IPS-матрицы ILI9488 обычно требуют инверсии цвета.
// Если на экране негатив (чёрный фон стал белым) — поменять на false.
constexpr bool kTftInvert = true;
// SCK через GPIO matrix: начинать с 27 МГц, повышать только при стабильной картинке.
constexpr uint32_t kTftSpiHz = 27000000;

// H1 запаян и джойстик подключён. Пока false — листание кнопкой BOOT.
constexpr bool kControlsConnected = false;
// Активный уровень кнопок джойстика не подтверждён: true — замыкание на GND
// (INPUT_PULLUP), false — на +3.3 В (INPUT_PULLDOWN). Проверить на железе.
constexpr bool kJoyActiveLow = true;

// Дамп всех кадров CAN в Serial в формате candump -L (читается SavvyCAN / can-utils)
constexpr bool kSerialDump = true;

}  // namespace cfg
