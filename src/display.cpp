#include "display.h"
#include "fonts_data.h"

// Init sequence from Waveshare's Display_ST7789.cpp for this 2.8" panel.
// LovyanGFX applies MADCTL (rotation), COLMOD and inversion itself afterwards.
const uint8_t* Panel_WS28::getInitCommands(uint8_t listno) const {
  static constexpr uint8_t list0[] = {
    0x11, 0 + CMD_INIT_DELAY, 120,               // sleep out
    0xB0, 2, 0x00, 0xE0,                          // RAM control: big-endian pixels (LovyanGFX order)
    0xB2, 5, 0x0C, 0x0C, 0x00, 0x33, 0x33,        // porch
    0xB7, 1, 0x75,                                // gate control
    0xBB, 1, 0x1A,                                // VCOM
    0xC0, 1, 0x2C,
    0xC2, 2, 0x01, 0xFF,
    0xC3, 1, 0x13,
    0xC4, 1, 0x20,
    0xC6, 1, 0x0F,                                // 60 Hz
    0xD0, 2, 0xA4, 0xA1,
    0xD6, 1, 0xA1,
    0xE0, 14, 0xD0, 0x0D, 0x14, 0x0D, 0x0D, 0x09, 0x38, 0x44, 0x4E, 0x3A, 0x17, 0x18, 0x2F, 0x30,
    0xE1, 14, 0xD0, 0x09, 0x0F, 0x08, 0x07, 0x14, 0x37, 0x44, 0x4D, 0x38, 0x15, 0x16, 0x2C, 0x2E,
    0x29, 0 + CMD_INIT_DELAY, 20,                 // display on
    0xFF, 0xFF,
  };
  return listno == 0 ? list0 : nullptr;
}

LGFX::LGFX() {
  {
    auto c = _bus.config();
    c.spi_host = SPI2_HOST;
    c.spi_mode = 0;
    c.freq_write = cfg::LCD_SPI_HZ;
    c.freq_read = 16000000;
    c.spi_3wire = false;
    c.use_lock = true;
    c.dma_channel = SPI_DMA_CH_AUTO;
    c.pin_sclk = cfg::PIN_LCD_SCLK;
    c.pin_mosi = cfg::PIN_LCD_MOSI;
    c.pin_miso = -1;
    c.pin_dc = cfg::PIN_LCD_DC;
    _bus.config(c);
    _panel.setBus(&_bus);
  }
  {
    auto c = _panel.config();
    c.pin_cs = cfg::PIN_LCD_CS;
    c.pin_rst = -1;       // reset is on the I/O expander (board::resetLcd)
    c.pin_busy = -1;
    c.panel_width = 240;
    c.panel_height = 320;
    c.memory_width = 240;
    c.memory_height = 320;
    c.offset_x = 0;
    c.offset_y = 0;
    c.offset_rotation = 0;
    c.readable = false;
    c.invert = true;
    c.rgb_order = true;   // panel is BGR
    c.dlen_16bit = false;
    c.bus_shared = false;
    _panel.config(c);
  }
  setPanel(&_panel);
}

namespace display {

LGFX lcd;

namespace {
struct FontSlot {
  const uint8_t* data;
  size_t len;
  lgfx::PointerWrapper wrap;
  lgfx::VLWfont vlw;
};
FontSlot s_fonts[F_COUNT] = {
  {vlw_ui8, sizeof(vlw_ui8), {}, {}},
  {vlw_ui9, sizeof(vlw_ui9), {}, {}},
  {vlw_ui11, sizeof(vlw_ui11), {}, {}},
  {vlw_ui13, sizeof(vlw_ui13), {}, {}},
  {vlw_ui17, sizeof(vlw_ui17), {}, {}},
  {vlw_temp, sizeof(vlw_temp), {}, {}},
  {vlw_mono8, sizeof(vlw_mono8), {}, {}},
  {vlw_mono11, sizeof(vlw_mono11), {}, {}},
  {vlw_mono15, sizeof(vlw_mono15), {}, {}},
};
bool s_loaded[F_COUNT] = {};
}  // namespace

void begin(bool flip) {
  lcd.init();
  lcd.setRotation(flip ? 2 : 0);
  lcd.fillScreen(TFT_BLACK);
  for (int i = 0; i < F_COUNT; ++i) {
    s_fonts[i].wrap.set(s_fonts[i].data, s_fonts[i].len);
    s_loaded[i] = s_fonts[i].vlw.loadFont(&s_fonts[i].wrap);
  }
}

void setFlip(bool flip) { lcd.setRotation(flip ? 2 : 0); }

const lgfx::IFont* font(FontId id) {
  if (id < F_COUNT && s_loaded[id]) return &s_fonts[id].vlw;
  return &fonts::Font0;
}

}  // namespace display

void fitText(Canvas& c, char* str, size_t cap, FontId f, int maxW) {
  if (c.textWidth(str, f) <= maxW) return;
  size_t n = strnlen(str, cap);
  while (n > 1) {
    --n;
    // never cut a multi-byte UTF-8 character in half
    while (n > 0 && ((uint8_t)str[n] & 0xC0) == 0x80) --n;
    // trim trailing spaces before adding the ellipsis
    while (n > 1 && str[n - 1] == ' ') --n;
    if (n + 4 > cap) continue;
    memcpy(str + n, "...", 3);
    str[n + 3] = 0;
    if (c.textWidth(str, f) <= maxW) return;
  }
}
