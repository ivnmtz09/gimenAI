#pragma once
#include <LovyanGFX.hpp>
#include "config.h"
#include "expressions.h"

// Configuración de hardware para ST7789 con LovyanGFX
class LGFX_ST7789 : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789      _panel_instance;
    lgfx::Bus_SPI           _bus_instance;

public:
    LGFX_ST7789() {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = SPI2_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 40000000;
            cfg.freq_read  = 16000000;
            cfg.pin_sclk = PIN_TFT_SCK;
            cfg.pin_mosi = PIN_TFT_MOSI;
            cfg.pin_miso = -1;
            cfg.pin_dc   = PIN_TFT_DC;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs           = PIN_TFT_CS;
            cfg.pin_rst          = PIN_TFT_RST;
            cfg.pin_busy         = -1;
            cfg.panel_width      = 240;
            cfg.panel_height     = 240;
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.bus_shared       = false;
            _panel_instance.config(cfg);
        }
        setPanel(&_panel_instance);
    }
};

class DisplayController {
private:
    LGFX_ST7789 tft;
    Expression currentExpression;
    unsigned long lastBlinkTime;
    bool isBlinking;

    void drawEye(int cx, int cy, int r, uint16_t color);
    void drawHappyEye(int cx, int cy, int r, uint16_t color);
    void drawSurprisedEye(int cx, int cy, int r, uint16_t color);
    void drawSadEye(int cx, int cy, int r, uint16_t color);

public:
    DisplayController();
    void begin();
    void setExpression(Expression expr);
    void updateAnimation();
    void showMessage(const char* text);
};
