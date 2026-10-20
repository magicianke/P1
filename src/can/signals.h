#pragma once
#include <stdint.h>

// Сигналы штатной шины GH для приборки: все ID, байты, маски и формулы — только здесь.
// Чистые функции без зависимостей от железа (собираются и на ПК).
// Источник — docs/can_ids.md. Пока все сигналы в разделе «Кандидаты»
// (сессия 2026-10-14), в «Подтверждённые» не переносились.
//
// tools/replay.ps1 читает константы kXxx прямо из этого файла:
// формат строк «constexpr <тип> kИмя = <число>;» не менять.
namespace sig {

// Обороты: 0x410 B5–B6, little-endian, об/мин. Кандидат, уверенность высокая.
constexpr uint32_t kRpmId = 0x410;
constexpr uint8_t kRpmLoByte = 5;
constexpr uint8_t kRpmHiByte = 6;

// Температура ОЖ: 0x600 B3 − 40, °C. Кандидат, уверенность средняя
// (со стрелкой и OBD PID 0x05 не сверено).
constexpr uint32_t kCoolantId = 0x600;
constexpr uint8_t kCoolantByte = 3;
constexpr int kCoolantOffset = 40;

// Селектор АКПП: 0x422 B2. Кандидат, уверенность высокая (положения 3/2/1 не проверены).
constexpr uint32_t kSelectorId = 0x422;
constexpr uint8_t kSelectorByte = 2;
constexpr uint8_t kSelectorCodeP = 0x07;
constexpr uint8_t kSelectorCodeR = 0x06;
constexpr uint8_t kSelectorCodeN = 0x05;
constexpr uint8_t kSelectorCodeD = 0x04;
constexpr uint8_t kSelectorCodeBetween = 0x08;  // между положениями: не менять показ

// Педаль тормоза: 0x411 B6 & 0x10, 1 = нажата. Кандидат, уверенность высокая.
constexpr uint32_t kBrakeId = 0x411;
constexpr uint8_t kBrakeByte = 6;
constexpr uint8_t kBrakeMask = 0x10;

// Водительская дверь: 0x620 B2 & 0x02, 0 = открыта. Кандидат, уверенность высокая.
constexpr uint32_t kDoorId = 0x620;
constexpr uint8_t kDoorByte = 2;
constexpr uint8_t kDoorClosedMask = 0x02;

// Нет кадров дольше — значение устарело, на экране «--».
constexpr uint32_t kStaleMs = 1000;

// Пороги цвета ОЖ, °C: ниже Cold — синий, выше Hot — красный, между — белый.
constexpr int kCoolantColdC = 60;
constexpr int kCoolantHotC = 100;

enum class Selector : uint8_t { Unknown, P, R, N, D };

constexpr uint16_t decodeRpm(const uint8_t* d) {
  return (uint16_t)(d[kRpmLoByte] | (d[kRpmHiByte] << 8));
}
constexpr int decodeCoolantC(const uint8_t* d) { return d[kCoolantByte] - kCoolantOffset; }
constexpr bool decodeBrake(const uint8_t* d) { return (d[kBrakeByte] & kBrakeMask) != 0; }
constexpr bool decodeDoorOpen(const uint8_t* d) { return (d[kDoorByte] & kDoorClosedMask) == 0; }

// Код B2 → положение. 0x08 и неизвестные коды → Unknown
// (0x08 «между положениями» отдельно обрабатывает decode()).
constexpr Selector decodeSelector(uint8_t code) {
  return code == kSelectorCodeP   ? Selector::P
         : code == kSelectorCodeR ? Selector::R
         : code == kSelectorCodeN ? Selector::N
         : code == kSelectorCodeD ? Selector::D
                                  : Selector::Unknown;
}

// Значение сигнала и время последнего кадра его ID
template <typename T>
struct Value {
  T v{};
  bool seen = false;  // был хоть один кадр
  uint32_t ms = 0;    // время последнего кадра

  bool fresh(uint32_t now) const { return seen && now - ms <= kStaleMs; }
};

struct DashState {
  Value<uint16_t> rpm;
  Value<int> coolant_c;
  Value<Selector> selector;  // последнее известное положение; ms — по любому кадру 0x422
  Value<bool> brake;
  Value<bool> door_open;
};

// Разбор одного кадра (11-битный ID). Кадры с коротким DLC пропускаются.
inline void decode(DashState& s, uint32_t id, uint8_t dlc, const uint8_t* d, uint32_t now) {
  auto put = [now](auto& val, auto v) {
    val.v = v;
    val.seen = true;
    val.ms = now;
  };
  switch (id) {
    case kRpmId:
      if (dlc > kRpmHiByte) put(s.rpm, decodeRpm(d));
      break;
    case kCoolantId:
      if (dlc > kCoolantByte) put(s.coolant_c, decodeCoolantC(d));
      break;
    case kSelectorId:
      if (dlc > kSelectorByte) {
        Selector sel = decodeSelector(d[kSelectorByte]);
        // 0x08 (между положениями) — оставляем последнее известное
        if (d[kSelectorByte] == kSelectorCodeBetween) sel = s.selector.v;
        put(s.selector, sel);
      }
      break;
    case kBrakeId:
      if (dlc > kBrakeByte) put(s.brake, decodeBrake(d));
      break;
    case kDoorId:
      if (dlc > kDoorByte) put(s.door_open, decodeDoorOpen(d));
      break;
  }
}

}  // namespace sig
