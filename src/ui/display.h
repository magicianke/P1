#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "../board_config.h"

// ILI9488 320x480 по SPI + резистивный тач XPT2046.
// В режиме SPI ILI9488 принимает только 18-битный цвет — LovyanGFX конвертирует сам.
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9488 _panel;
  lgfx::Bus_SPI _bus;
  lgfx::Light_PWM _light;
  lgfx::Touch_XPT2046 _touch;

 public:
  LGFX() {
    {
      auto cfg = _bus.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = cfg::kTftSpiHz;
      cfg.freq_read = 16000000;
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
      cfg.readable = false;  // SDO не подключён: ILI9488 не отпускает линию MISO
      cfg.invert = cfg::kTftInvert;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = true;
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
    {
      auto cfg = _touch.config();
      // Сырые значения АЦП; точная калибровка — позже через calibrateTouch()
      cfg.x_min = 300;
      cfg.x_max = 3800;
      cfg.y_min = 300;
      cfg.y_max = 3800;
      cfg.pin_int = pins::kTouchIrq;
      cfg.bus_shared = true;
      cfg.offset_rotation = 0;
      cfg.spi_host = SPI2_HOST;
      cfg.freq = 1000000;
      cfg.pin_sclk = pins::kTftSclk;
      cfg.pin_mosi = pins::kTftMosi;
      cfg.pin_miso = pins::kTftMiso;
      cfg.pin_cs = pins::kTouchCs;
      _touch.config(cfg);
      _panel.setTouch(&_touch);
    }
    setPanel(&_panel);
  }
};

extern LGFX tft;

namespace ui {
void displayBegin();
}
