#pragma once
#include <stdint.h>

// Плата: ESP32-S3 DevKitC-1 (клон, два разъёма Type-C: "COM" — USB-UART, "USB" — нативный USB).
// НЕ использовать:
//   GPIO0, 3, 45, 46 — strapping-пины
//   GPIO19, 20       — нативный USB (D-/D+)
//   GPIO26–32        — SPI flash
//   GPIO33–37        — OPI PSRAM на модулях N*R8
//   GPIO43, 44       — UART0 (мост USB-UART)
//   GPIO38/48        — RGB-светодиод на плате (зависит от ревизии)

namespace pins {

// Дисплей ILI9488 (SPI2/FSPI, пины IOMUX — максимальная частота)
constexpr int kTftSclk = 12;  // SCK + T_CLK
constexpr int kTftMosi = 11;  // SDI + T_DIN
constexpr int kTftMiso = 13;  // только T_DO! SDO дисплея не подключать
constexpr int kTftCs   = 10;
constexpr int kTftDc   = 9;
constexpr int kTftRst  = 14;
constexpr int kTftBl   = 21;  // LED (подсветка, ШИМ)

// Тач XPT2046 (общая шина SPI с дисплеем)
constexpr int kTouchCs  = 15;
constexpr int kTouchIrq = 16;

// CAN: SN65HVD230 (D — TX, R — RX)
constexpr int kCanTx = 5;
constexpr int kCanRx = 4;

}  // namespace pins

namespace cfg {

// IPS-матрицы ILI9488 обычно требуют инверсии цвета.
// Если на экране негатив (чёрный фон стал белым) — поменять на false.
constexpr bool kTftInvert = true;
constexpr uint32_t kTftSpiHz = 40000000;

// Дамп всех кадров CAN в Serial в формате candump -L (читается SavvyCAN / can-utils)
constexpr bool kSerialDump = true;

}  // namespace cfg
