#include "settings.h"
#include <Preferences.h>
#include <stddef.h>

Settings g_set;

namespace {
constexpr uint32_t MAGIC = 0x52414452;  // "RADR"
constexpr uint16_t VERSION = 1;

void defaults(Settings& s) {
  memset(&s, 0, sizeof(s));
  s.magic = MAGIC;
  s.version = VERSION;
  s.locMode = 0;
  s.locValid = false;
  s.popupIdx = 1;        // 5 s
  s.rangeIdx = 2;        // 10 mi
  s.labels = LBL_CALLSIGN | LBL_TYPE | LBL_ALT;
  s.hideGround = true;
  s.runways = true;
  s.trails = false;
  s.altFilter = ALT_ALL;
  s.source = SRC_ADSBFI;
  s.brightness = 70;
  s.units = U_IMPERIAL;
  s.theme = T_CLASSIC;
  s.clock24 = false;
  s.nightDim = true;
  s.flip = false;
  s.chime = false;
  s.santa = true;
  s.nightStart = 22 * 60;
  s.nightEnd = 6 * 60;
  s.nightBright = 15;
  s.tapWake = true;
  s.milHighlight = true;
  s.milColor = 0;  // lavender
}
}  // namespace

void settingsLoad() {
  defaults(g_set);
  Preferences p;
  if (!p.begin("radar", true)) return;
  Settings tmp;
  defaults(tmp);
  size_t stored = p.getBytesLength("cfg");
  size_t n = 0;
  // Older firmware saved a shorter struct. Recognise it by size, keep the fields
  // it had, and leave the newer ones at their defaults.
  auto padded = [](size_t len) { return (len + alignof(Settings) - 1) / alignof(Settings) * alignof(Settings); };
  size_t keep = 0;
  // v1.0.0-1.0.3 and v1.0.4-1.0.6 happen to be the same size (the night fields
  // fit in what was padding), so the night fields are checked separately below.
  if (stored == sizeof(tmp)) keep = sizeof(tmp);
  else if (stored == padded(offsetof(Settings, milHighlight))) keep = offsetof(Settings, milHighlight);
  else if (stored == padded(offsetof(Settings, nightStart))) keep = offsetof(Settings, nightStart);
  if (keep) {
    uint8_t* raw = (uint8_t*)malloc(stored);
    if (raw) {
      if (p.getBytes("cfg", raw, stored) == stored) {
        memcpy(&tmp, raw, keep);
        n = sizeof(tmp);
        // A pre-v1.0.4 save reads as all-zero night fields: use the defaults instead.
        if (keep < sizeof(tmp) && tmp.nightStart == 0 && tmp.nightEnd == 0 && tmp.nightBright == 0 && !tmp.tapWake) {
          tmp.nightStart = 22 * 60;
          tmp.nightEnd = 6 * 60;
          tmp.nightBright = 15;
          tmp.tapWake = true;
        }
      }
      free(raw);
    }
  }
  p.end();
  if (n == sizeof(tmp) && tmp.magic == MAGIC && tmp.version == VERSION) {
    g_set = tmp;
    if (g_set.nightStart >= 1440) g_set.nightStart = 22 * 60;
    if (g_set.nightEnd >= 1440) g_set.nightEnd = 6 * 60;
    if (g_set.nightBright > 60) g_set.nightBright = 15;
    if (g_set.milColor >= MIL_COLOR_COUNT) g_set.milColor = 0;
    g_set.ssid[sizeof(g_set.ssid) - 1] = 0;
    g_set.pass[sizeof(g_set.pass) - 1] = 0;
    g_set.zip[sizeof(g_set.zip) - 1] = 0;
    g_set.place[sizeof(g_set.place) - 1] = 0;
    if (g_set.popupIdx >= POPUP_COUNT) g_set.popupIdx = 1;
    if (g_set.rangeIdx >= RANGE_COUNT) g_set.rangeIdx = 2;
    if (g_set.source >= SRC_COUNT) g_set.source = SRC_ADSBFI;
    if (g_set.units > U_AVIATION) g_set.units = U_IMPERIAL;
    if (g_set.theme > T_HOLIDAY) g_set.theme = T_CLASSIC;
    if (g_set.brightness < 10 || g_set.brightness > 100) g_set.brightness = 70;
  }
}

void settingsSave() {
  Preferences p;
  if (!p.begin("radar", false)) return;
  p.putBytes("cfg", &g_set, sizeof(g_set));
  p.end();
}

void settingsFactoryReset() {
  Preferences p;
  if (p.begin("radar", false)) {
    p.clear();
    p.end();
  }
  delay(100);
  ESP.restart();
}

int rangeValue(uint8_t units, int idx) {
  static const int imp[RANGE_COUNT] = {3, 5, 10, 25, 50};
  static const int met[RANGE_COUNT] = {5, 10, 20, 40, 80};
  if (idx < 0) idx = 0;
  if (idx >= RANGE_COUNT) idx = RANGE_COUNT - 1;
  return units == U_METRIC ? met[idx] : imp[idx];
}

const char* distUnit(uint8_t units) {
  return units == U_METRIC ? "km" : units == U_AVIATION ? "nm" : "mi";
}

float unitsPerNm(uint8_t units) {
  return units == U_METRIC ? 1.852f : units == U_AVIATION ? 1.0f : 1.15078f;
}

const char* sourceName(uint8_t src) {
  switch (src) {
    case SRC_AIRPLANESLIVE: return "airplanes.live";
    case SRC_ADSBLOL: return "adsb.lol";
    default: return "adsb.fi";
  }
}

uint16_t militaryColor() {
  const NamedColor& c = MIL_COLORS[g_set.milColor < MIL_COLOR_COUNT ? g_set.milColor : 0];
  return (uint16_t)(((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.b >> 3));
}
