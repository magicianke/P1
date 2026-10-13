# Первый запуск платы P1 rev 1.0

Дата: 2026-10-13. Протокол первого включения платы с модулем ESP32-S3-DevKitC-1 N16R8 (клон).

## Условия
- Модуль вставлен в гнёзда платы P1 rev 1.0.
- USB подключён к разъёму модуля с подписью **USB** (нативный USB ESP32-S3).
- 12 В **не подключено**, машины нет.

## 1. Окружение
| Компонент | Версия | Статус |
|---|---|---|
| arduino-cli (из Arduino IDE 2, путь как в `build.ps1`) | 1.4.1 | OK |
| Ядро esp32:esp32 | 3.3.11 (доступна 3.3.12, не обновляли) | OK |
| LovyanGFX | 1.2.27 (доступна 1.2.32, не обновляли) | OK |

Из внешних библиотек код подключает только **LovyanGFX**. `driver/twai.h`, `freertos/semphr.h` и `Arduino.h` входят в ядро.

## 2. Компиляция (`.\build.ps1 compile`)
Ошибок и предупреждений нет. Прошивка 375 559 байт (11% раздела на 3 МБ), глобальные переменные 43 820 байт (13% RAM).

## 3. Порт (`arduino-cli board list`)
- Виден один порт — **COM3**, USB VID `303A`, PID `1001`: нативный USB Serial/JTAG ESP32-S3.
- arduino-cli подписывает его ещё и как «Ozobot DRVKit»: у той платы такие же VID/PID. Отдельного устройства нет.
- BOOT/RST нажимать не понадобилось.

## 4. Chipinfo (`.\build.ps1 chipinfo -Port COM3`)
```
Chip type:  ESP32-S3 (QFN56) (revision v0.2)
Features:   Wi-Fi, BT 5 (LE), Dual Core + LP Core, 240MHz, Embedded PSRAM 8MB (AP_3v3)
USB mode:   USB-Serial/JTAG
Detected flash size: 16MB
Flash type set in eFuse: quad (4 data lines)
Flash voltage set by eFuse: 3.3V
```
Flash 16 МБ и PSRAM 8 МБ совпадают с N16R8. PSRAM встроенная октальная, поэтому настройка `PSRAM=opi` верная.

## 5. Прошивка (`.\build.ps1 upload -Port COM3`)
Записаны bootloader, таблица разделов, boot_app0 и приложение. Хеши всех образов сошлись (`Hash of data verified`), плата перезагрузилась.

## 6. Монитор порта (2 000 000 бод, ~20 с)
```
# P1 alive 5s rx:0 fr/s:0 miss:0 err:0 drop:0
# P1 alive 10s rx:0 fr/s:0 miss:0 err:0 drop:0
# P1 alive 15s rx:0 fr/s:0 miss:0 err:0 drop:0
# P1 alive 20s rx:0 fr/s:0 miss:0 err:0 drop:0
# P1 alive 25s rx:0 fr/s:0 miss:0 err:0 drop:0
# P1 alive 30s rx:0 fr/s:0 miss:0 err:0 drop:0
# P1 alive 35s rx:0 fr/s:0 miss:0 err:0 drop:0
```
- Строки `# P1 TWAI init failed` нет. Если бы TWAI не запустился, строк `alive` не было бы, значит драйвер TWAI запущен.
- `rx:0` и `err:0` ожидаемы без машины и без 12 В.
- Строки начинаются с 5 с, хотя монитор работал 20 с: плата копит вывод в TX-буфере USB CDC (16 КБ) и выдаёт всё накопленное при подключении монитора.

### Заметка: монитор из скрипта
`arduino-cli monitor` сразу завершается, если stdin закрыт (`< /dev/null`). Для запуска с таймаутом без интерактивного терминала держите stdin открытым:
```bash
(sleep 25) | timeout 20 "$CLI" monitor -p COM3 -c baudrate=2000000 --quiet
```

## 7. Настройки Arduino IDE (Tools), эквивалентные `build.ps1`
| Пункт меню | Значение | Опция FQBN |
|---|---|---|
| Board | ESP32S3 Dev Module | `esp32:esp32:esp32s3` |
| USB CDC On Boot | Enabled | `CDCOnBoot=cdc` |
| USB Mode | Hardware CDC and JTAG | `USBMode=hwcdc` |
| Flash Size | 16MB (128Mb) | `FlashSize=16M` |
| PSRAM | OPI PSRAM | `PSRAM=opi` |
| Partition Scheme | 16M Flash (3MB APP/9.9MB FATFS) | `PartitionScheme=app3M_fat9M_16MB` |
| Upload Mode | UART0 / Hardware CDC | по умолчанию |
| CPU Frequency | 240MHz (WiFi) | `CPUFreq=240` |
| Flash Mode | QIO 80MHz | по умолчанию |
| Upload Speed | 921600 | по умолчанию |
| Port | COM3 | — |

Скорость в Serial Monitor — 2000000. Для нативного USB она ни на что не влияет.

## Итог
Плата работает от USB: прошивка заливается, TWAI запускается в listen-only, heartbeat идёт.
Что ещё не проверено: питание 12 В через LM2596, дисплей и приём CAN на машине.
