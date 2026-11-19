// Colors for the radar themes and the settings UI (RGB565)
#pragma once
#include <stdint.h>

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

struct RadarTheme {
  uint16_t bg;          // radar background
  uint16_t cross;       // crosshair
  uint16_t ring;        // outer ring
  uint16_t ringDim;     // inner rings
  uint16_t ringLabel;   // ring distance labels
  uint16_t compass;     // N/E/S/W
  uint16_t plane;       // aircraft
  uint16_t planeSel;    // tapped aircraft
  uint16_t label;       // callsign
  uint16_t labelSub;    // type / altitude
  uint16_t vector;      // speed vector
  uint16_t trail;       // flight trail
  uint16_t runway;      // runways
  uint16_t runwayLabel; // airport code
  uint16_t home;        // center marker
};

// Classic: navy scope, phosphor rings, amber planes (the mockup look)
constexpr RadarTheme THEME_CLASSIC = {
  rgb(7, 20, 35), rgb(15, 46, 34), rgb(30, 87, 67), rgb(26, 76, 59), rgb(95, 168, 137),
  rgb(220, 230, 240), rgb(255, 181, 71), rgb(92, 214, 255), rgb(242, 246, 250),
  rgb(157, 176, 195), rgb(255, 106, 213), rgb(120, 96, 50), rgb(46, 196, 182),
  rgb(87, 211, 199), rgb(255, 255, 255)
};

// Amber: warm monochrome CRT look
constexpr RadarTheme THEME_AMBER = {
  rgb(18, 10, 2), rgb(48, 28, 6), rgb(140, 84, 20), rgb(100, 60, 14), rgb(200, 130, 50),
  rgb(255, 214, 150), rgb(255, 196, 92), rgb(255, 255, 255), rgb(255, 228, 180),
  rgb(200, 150, 90), rgb(255, 140, 40), rgb(110, 70, 20), rgb(170, 110, 40),
  rgb(220, 160, 80), rgb(255, 230, 190)
};

// Holiday: deep pine green, candy-cane red rings, white planes, snow
constexpr RadarTheme THEME_HOLIDAY = {
  rgb(6, 26, 18), rgb(16, 52, 36), rgb(200, 50, 60), rgb(150, 40, 48), rgb(240, 150, 150),
  rgb(255, 240, 240), rgb(240, 248, 255), rgb(255, 215, 90), rgb(255, 255, 255),
  rgb(170, 210, 180), rgb(255, 215, 90), rgb(60, 110, 80), rgb(61, 220, 132),
  rgb(120, 230, 160), rgb(255, 215, 90)
};

// Settings / strip UI palette
namespace ui_c {
constexpr uint16_t SCREEN_BG   = rgb(10, 20, 32);
constexpr uint16_t HEADER_BG   = rgb(11, 26, 43);
constexpr uint16_t BORDER      = rgb(27, 48, 72);
constexpr uint16_t ROW_BG      = rgb(14, 28, 45);
constexpr uint16_t DIVIDER     = rgb(27, 47, 69);
constexpr uint16_t ICON_BOX    = rgb(21, 48, 74);
constexpr uint16_t ICON        = rgb(140, 200, 255);
constexpr uint16_t TEXT        = rgb(232, 238, 244);
constexpr uint16_t TEXT_DIM    = rgb(143, 166, 189);
constexpr uint16_t TEXT_FAINT  = rgb(93, 118, 144);
constexpr uint16_t CHEVRON     = rgb(111, 138, 166);
constexpr uint16_t BTN_BG      = rgb(18, 38, 60);
constexpr uint16_t BTN_PRESSED = rgb(36, 72, 110);
constexpr uint16_t KEY_BG      = rgb(21, 40, 62);
constexpr uint16_t KEY_ALT     = rgb(28, 58, 90);
constexpr uint16_t KEY_BORDER  = rgb(36, 64, 94);
constexpr uint16_t SEG_ON      = rgb(31, 68, 102);
constexpr uint16_t GREEN       = rgb(61, 220, 132);
constexpr uint16_t GREEN_DARK  = rgb(6, 32, 18);
constexpr uint16_t TOGGLE_ON   = rgb(47, 181, 108);
constexpr uint16_t TOGGLE_OFF  = rgb(42, 61, 83);
constexpr uint16_t CYAN        = rgb(92, 214, 255);
constexpr uint16_t AMBER       = rgb(255, 181, 71);
constexpr uint16_t RED         = rgb(255, 92, 92);
constexpr uint16_t STRIP_BG    = rgb(11, 26, 43);
constexpr uint16_t HUD_BG      = rgb(5, 11, 20);
constexpr uint16_t HUD_TEXT    = rgb(207, 227, 247);
constexpr uint16_t PHOTO_BG    = rgb(19, 38, 58);
constexpr uint16_t PHOTO_BORDER= rgb(36, 64, 94);
}  // namespace ui_c
