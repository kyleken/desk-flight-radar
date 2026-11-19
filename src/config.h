// Desk Flight Radar - compile-time configuration
#pragma once
#include <Arduino.h>

#define FW_VERSION "1.0.12"

namespace cfg {

// ---------------------------------------------------------------------------
// Waveshare ESP32-C6-Touch-LCD-2.8 pins (from the Waveshare board docs)
// ---------------------------------------------------------------------------
constexpr int PIN_LCD_SCLK = 0;
constexpr int PIN_LCD_MOSI = 1;
constexpr int PIN_LCD_DC   = 10;
constexpr int PIN_LCD_CS   = 11;
constexpr int PIN_SD_CS    = 23;   // shares the SPI bus; held high (unused)
constexpr int PIN_I2C_SDA  = 6;
constexpr int PIN_I2C_SCL  = 7;
constexpr int PIN_TP_INT   = 18;
constexpr int PIN_BOOT     = 9;

constexpr int PIN_I2S_BCLK = 15;
constexpr int PIN_I2S_WS   = 3;
constexpr int PIN_I2S_DOUT = 2;
constexpr int PIN_I2S_DIN  = 20;

// I2C devices
constexpr uint8_t I2C_EXPANDER = 0x24;  // CH32V003 I/O expander
constexpr uint8_t I2C_TOUCH    = 0x58;  // CST3530
constexpr uint8_t I2C_CODEC    = 0x18;  // ES8311 (0x30 in 8-bit form)

// CH32V003 expander pins / registers
constexpr uint8_t EXIO_TP_RST  = 0;
constexpr uint8_t EXIO_LCD_RST = 1;
constexpr uint8_t EXIO_PA_EN   = 3;
constexpr uint8_t EXP_REG_MODE = 0x02;
constexpr uint8_t EXP_REG_OUT  = 0x03;
constexpr uint8_t EXP_REG_PWM  = 0x05;
constexpr uint8_t EXP_REG_ADC  = 0x06;

constexpr uint32_t LCD_SPI_HZ = 40000000;

// ---------------------------------------------------------------------------
// Screen geometry: 240x320 portrait, ~0.18 mm per pixel.
// The radar is the top 240x240 (43.2 x 43.2 mm); the strip is 240x80 below.
// ---------------------------------------------------------------------------
constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 320;
constexpr int RADAR_SIZE = 240;
constexpr int STRIP_Y = 240;
constexpr int STRIP_H = 80;
constexpr int BAND_H = 80;          // render in 4 bands of 240x80 (38 KB)
constexpr int RADAR_C = 120;        // radar center
constexpr int RING_R = 108;         // outer ring radius = selected range
constexpr int EDGE_LIM = 113;       // planes beyond this become edge dots

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
constexpr uint32_t ADSB_INTERVAL_MS    = 5000;        // public API limit is 1 req/s
constexpr uint32_t WEATHER_INTERVAL_MS = 10 * 60 * 1000;
constexpr uint32_t RADAR_FRAME_MS      = 200;         // 5 fps redraw (dead-reckoned)
constexpr uint32_t TOUCH_POLL_MS       = 15;
constexpr uint32_t STALE_AFTER_MS      = 60000;       // drop planes not seen for 60 s

constexpr int MAX_AIRCRAFT = 100;
constexpr int TRAIL_POINTS = 12;                      // 12 x 5 s = 60 s of trail

constexpr size_t PHOTO_MAX_BYTES = 40 * 1024;
constexpr int PHOTO_W = 88;
constexpr int PHOTO_H = 59;

constexpr const char* AP_PREFIX = "FlightRadar-";
// Identifies the device to the services it uses, with where to find out more
// (planespotters asks for a proper User-Agent).
constexpr const char* USER_AGENT =
    "DeskFlightRadar/" FW_VERSION " (ESP32-C6 desk gadget; +https://github.com/kyleken/desk-flight-radar)";

}  // namespace cfg
