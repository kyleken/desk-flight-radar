// Display (ST7789 via LovyanGFX), smooth fonts and a band-rendering canvas.
#pragma once
#include <LovyanGFX.hpp>
#include "config.h"

// ST7789 with Waveshare's init sequence for this panel.
class Panel_WS28 : public lgfx::Panel_ST7789 {
 public:
  const uint8_t* getInitCommands(uint8_t listno) const override;
};

class LGFX : public lgfx::LGFX_Device {
  Panel_WS28 _panel;
  lgfx::Bus_SPI _bus;
 public:
  LGFX();
};

enum FontId : uint8_t { F_UI8, F_UI9, F_UI11, F_UI13, F_UI17, F_TEMP, F_MONO8, F_MONO11, F_MONO15, F_COUNT };

namespace display {
extern LGFX lcd;
void begin(bool flip);
void setFlip(bool flip);
const lgfx::IFont* font(FontId id);
}  // namespace display

// A 240x80 sprite that stands in for one horizontal band of the screen.
// Screens draw in screen coordinates; the canvas shifts everything by -oy.
struct Canvas {
  lgfx::LGFX_Sprite* s;
  int oy;

  bool visible(int y, int h) const { return y + h > oy && y < oy + cfg::BAND_H; }

  void fillRect(int x, int y, int w, int h, uint16_t c) { s->fillRect(x, y - oy, w, h, c); }
  void drawRect(int x, int y, int w, int h, uint16_t c) { s->drawRect(x, y - oy, w, h, c); }
  void hline(int x, int y, int w, uint16_t c) { s->drawFastHLine(x, y - oy, w, c); }
  void vline(int x, int y, int h, uint16_t c) { s->drawFastVLine(x, y - oy, h, c); }
  void line(int x0, int y0, int x1, int y1, uint16_t c) { s->drawLine(x0, y0 - oy, x1, y1 - oy, c); }
  void wideLine(float x0, float y0, float x1, float y1, float r, uint16_t c) {
    s->drawWideLine(x0, y0 - oy, x1, y1 - oy, r, c);
  }
  void smoothLine(float x0, float y0, float x1, float y1, uint16_t c) {
    s->drawWideLine(x0, y0 - oy, x1, y1 - oy, 0.6f, c);
  }
  void circle(int x, int y, int r, uint16_t c) { s->drawCircle(x, y - oy, r, c); }
  void fillCircle(int x, int y, int r, uint16_t c) { s->fillCircle(x, y - oy, r, c); }
  void smoothCircle(int x, int y, int r, uint16_t c) { s->fillSmoothCircle(x, y - oy, r, c); }
  void ring(int x, int y, int r, float thick, uint16_t c) {
    s->fillArc(x, y - oy, r, r - (int)thick + 1, 0, 360, c);
  }
  void roundRect(int x, int y, int w, int h, int r, uint16_t c) { s->fillSmoothRoundRect(x, y - oy, w, h, r, c); }
  void roundFrame(int x, int y, int w, int h, int r, uint16_t c) { s->drawRoundRect(x, y - oy, w, h, r, c); }
  void tri(float x0, float y0, float x1, float y1, float x2, float y2, uint16_t c) {
    s->fillTriangle(lroundf(x0), lroundf(y0) - oy, lroundf(x1), lroundf(y1) - oy, lroundf(x2), lroundf(y2) - oy, c);
  }

  // Text. datum: lgfx textdatum (top_left, middle_left, middle_center, ...)
  int text(const char* str, int x, int y, FontId f, uint16_t color,
           lgfx::textdatum_t datum = lgfx::top_left) {
    s->setFont(display::font(f));
    s->setTextDatum(datum);
    s->setTextColor(color);
    return s->drawString(str, x, y - oy);
  }
  int textWidth(const char* str, FontId f) {
    s->setFont(display::font(f));
    return s->textWidth(str);
  }
};

// Shorten str (in place) with an ellipsis so it fits maxW pixels in font f.
void fitText(Canvas& c, char* str, size_t cap, FontId f, int maxW);
