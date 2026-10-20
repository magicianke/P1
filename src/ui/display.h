#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include <type_traits>

#include "../board_config.h"

// Дисплей 320x480 по SPI, контроллер выбирается в cfg::kTftController.
// ILI9488 в режиме SPI принимает только 18-битный цвет, ST7796 — 16-битный;
// LovyanGFX конвертирует сам.
using TftPanel = std::conditional_t<cfg::kTftController == cfg::TftController::ST7796,
                                    lgfx::Panel_ST7796, lgfx::Panel_ILI9488>;

class LGFX : public lgfx::LGFX_Device {
  TftPanel _panel;
  lgfx::Bus_SPI _bus;
  lgfx::Light_PWM _light;

 public:
  LGFX() {
    {
      auto cfg = _bus.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = cfg::kTftSpiHz;
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = pins::kTftSclk;
      cfg.pin_mosi = pins::kTftMosi;
      cfg.pin_miso = pins::kTftMiso;
      cfg.pin_dc = pins::kTftDc;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs = pins::kTftCs;
      cfg.pin_rst = pins::kTftRst;
      cfg.pin_busy = -1;
      cfg.panel_width = 320;
      cfg.panel_height = 480;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.readable = false;  // MISO нет (SDO не подключён)
      cfg.invert = cfg::kTftInvert;
      cfg.rgb_order = !cfg::kTftBgr;  // в LovyanGFX false = бит BGR в MADCTL
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel.config(cfg);
    }
    {
      auto cfg = _light.config();
      cfg.pin_bl = pins::kTftBl;
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.setLight(&_light);
    }
    setPanel(&_panel);
  }
};

extern LGFX tft;

namespace ui {
// Инициализация и тестовый экран на cfg::kTftTestMs (блокирует только setup()).
void displayBegin();
}
