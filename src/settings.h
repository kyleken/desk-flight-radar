// User settings, stored in flash (NVS)
#pragma once
#include <Arduino.h>

enum Units : uint8_t { U_IMPERIAL = 0, U_METRIC = 1, U_AVIATION = 2 };
enum ThemeId : uint8_t { T_CLASSIC = 0, T_AMBER = 1, T_HOLIDAY = 2 };
enum AltFilter : uint8_t { ALT_ALL = 0, ALT_LOW = 1, ALT_HIGH = 2 };
enum LabelBits : uint8_t { LBL_CALLSIGN = 1, LBL_TYPE = 2, LBL_ALT = 4, LBL_SPEED = 8 };
enum Source : uint8_t { SRC_ADSBFI = 0, SRC_AIRPLANESLIVE = 1, SRC_ADSBLOL = 2, SRC_COUNT = 3 };

struct Settings {
  uint32_t magic;
  uint16_t version;

  char ssid[33];
  char pass[65];

  uint8_t locMode;      // 0 = ZIP, 1 = lat/long
  char zip[6];
  double lat;
  double lon;
  bool locValid;        // lat/lon known (ZIP resolved or typed)
  char place[40];       // "Chicago, IL" (from ZIP lookup)

  uint8_t popupIdx;     // index into POPUP_SECONDS
  uint8_t rangeIdx;     // start-up range, index into range presets
  uint8_t labels;       // LabelBits
  bool hideGround;
  bool runways;
  bool trails;
  uint8_t altFilter;    // AltFilter
  uint8_t source;       // Source

  uint8_t brightness;   // 10..100
  uint8_t units;        // Units
  uint8_t theme;        // ThemeId
  bool clock24;
  bool nightDim;
  bool flip;
  bool chime;
  bool santa;

  // added in v1.0.4 (older saves load with these defaults)
  uint16_t nightStart;  // minutes after midnight, local time
  uint16_t nightEnd;
  uint8_t nightBright;  // 0..60 %, 0 = screen off
  bool tapWake;         // touching the screen at night brightens it for 30 s

  // added in v1.0.7
  bool milHighlight;    // color military aircraft differently
  uint8_t milColor;     // index into MIL_COLORS
  uint8_t spare[6];     // keeps the saved size growing so old saves are recognized
};

extern Settings g_set;

void settingsLoad();
void settingsSave();
void settingsFactoryReset();   // erase everything (incl. Wi-Fi) and reboot

// --- option tables ---
constexpr uint8_t POPUP_SECONDS[] = {3, 5, 8, 10, 15, 30};
constexpr int POPUP_COUNT = sizeof(POPUP_SECONDS);
constexpr int RANGE_COUNT = 5;

// Military highlight colors (never red = emergency, cyan = selected, amber = normal)
struct NamedColor { const char* name; uint8_t r, g, b; };
constexpr NamedColor MIL_COLORS[] = {
  {"Lavender", 190, 150, 255}, {"Pink", 255, 120, 200}, {"White", 255, 255, 255}, {"Sky blue", 120, 200, 255},
  {"Lime", 190, 255, 70},      {"Green", 120, 232, 96}, {"Orange", 255, 140, 50}, {"Yellow", 255, 232, 70},
};
constexpr int MIL_COLOR_COUNT = sizeof(MIL_COLORS) / sizeof(MIL_COLORS[0]);
uint16_t militaryColor();   // RGB565 of the chosen color

// Range presets in the user's distance unit
int rangeValue(uint8_t units, int idx);
const char* distUnit(uint8_t units);          // "mi" / "km" / "nm"
float unitsPerNm(uint8_t units);              // 1 nm in the display unit
const char* sourceName(uint8_t src);
