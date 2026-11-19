// Settings menu, Radar options, Display/units/sound options, About
#include <WiFi.h>
#include "audio.h"
#include "board.h"
#include "icons.h"
#include "net.h"
#include "portal.h"
#include "settings.h"
#include "state.h"
#include "ui.h"

extern uint32_t g_santaPreviewUntil;
extern bool g_applyStartupRange;

namespace ui {
namespace {

void icPin(Canvas& c, int x, int y) { icons::pin(c, x, y, 14, ui_c::ICON, ui_c::ICON_BOX); }
void icWifi(Canvas& c, int x, int y) { icons::wifi(c, x, y + 5, 14, 4, ui_c::ICON, ui_c::ICON); }
void icRadar(Canvas& c, int x, int y) { icons::radar(c, x, y, 14, ui_c::ICON, ui_c::ICON_BOX); }
void icSun(Canvas& c, int x, int y) { icons::sun(c, x, y, 3, ui_c::ICON); }
void icInfo(Canvas& c, int x, int y) { icons::info(c, x, y, 7, ui_c::ICON, ui_c::ICON_BOX); }
void icStar(Canvas& c, int x, int y) { icons::star(c, x, y, 8, ui_c::ICON); }

// Row with a label (+ optional sub label) on the left and a toggle on the right
void toggleRow(Canvas& c, int y, int h, const char* label, const char* sub, bool on) {
  if (!c.visible(y, h)) return;
  c.fillRect(0, y, 240, h, ui_c::ROW_BG);
  rowDivider(c, y + h - 1);
  int cy = y + h / 2;
  if (sub) {
    c.text(label, 8, cy - 5, F_UI11, ui_c::TEXT, lgfx::middle_left);
    c.text(sub, 8, cy + 7, F_UI8, ui_c::TEXT_DIM, lgfx::middle_left);
  } else {
    c.text(label, 8, cy, F_UI11, ui_c::TEXT, lgfx::middle_left);
  }
  toggle(c, 202, cy - 8, on);
}

void rowBg(Canvas& c, int y, int h) {
  c.fillRect(0, y, 240, h, ui_c::ROW_BG);
  rowDivider(c, y + h - 1);
}

// =========================================================================
class SettingsScreen : public Screen {
  static constexpr int RH = 38;  // row height
  static const char* sub2() {
    static char b[48];
    snprintf(b, sizeof(b), "v" FW_VERSION "%s%s", g_net.ip[0] ? " \xC2\xB7 " : "", g_net.ip);
    return b;
  }

 public:
  void render(Canvas& c) override {
    c.fillRect(0, c.oy, 240, cfg::BAND_H, ui_c::SCREEN_BG);
    header(c, "Settings");
    char sub[64];
    int y = HEADER_H;

    if (!g_set.locValid && !(g_set.locMode == 0 && g_set.zip[0])) strcpy(sub, "Not set");
    else if (g_set.locMode == 0) snprintf(sub, sizeof(sub), "ZIP %s%s%s", g_set.zip, g_set.place[0] ? " \xC2\xB7 " : "", g_set.place);
    else snprintf(sub, sizeof(sub), "%.4f, %.4f", g_set.lat, g_set.lon);
    navRow(c, y, RH, icPin, "Location", sub);
    y += RH;

    const char* ws = g_net.wifi == W_CONNECTED ? "connected" : g_net.wifi == W_NO_CREDS ? "" : "connecting...";
    if (g_set.ssid[0]) snprintf(sub, sizeof(sub), "%s \xC2\xB7 %s", g_set.ssid, ws);
    else strcpy(sub, "Not set");
    navRow(c, y, RH, icWifi, "Wi-Fi", sub);
    y += RH;

    snprintf(sub, sizeof(sub), "%d %s \xC2\xB7 details %d s \xC2\xB7 labels %s", rangeValue(g_set.units, g_set.rangeIdx),
             distUnit(g_set.units), POPUP_SECONDS[g_set.popupIdx], g_set.labels ? "on" : "off");
    navRow(c, y, RH, icRadar, "Radar", sub);
    y += RH;

    static const char* themes[] = {"Classic", "Amber", "Holiday"};
    static const char* units[] = {"Imperial", "Metric", "Aviation"};
    snprintf(sub, sizeof(sub), "%s theme \xC2\xB7 %d%% \xC2\xB7 %s", themes[g_set.theme], g_set.brightness, units[g_set.units]);
    navRow(c, y, RH, icSun, "Display, units & sound", sub);
    y += RH;

    if (g_set.milHighlight) snprintf(sub, sizeof(sub), "Shown in %s", MIL_COLORS[g_set.milColor].name);
    else strcpy(sub, "Not highlighted");
    navRow(c, y, RH, icStar, "Military aircraft", sub);
    y += RH;

    navRow(c, y, RH, icInfo, "About & reset", sub2());
    y += RH;

    if (c.visible(266, 54)) {
      bool p = pressedIn(30, 271, 180, 28);
      c.roundRect(30, 271, 180, 28, 14, p ? ui_c::BTN_PRESSED : ui_c::SCREEN_BG);
      c.s->drawRoundRect(30, 271 - c.oy, 180, 28, 14, ui_c::BORDER);
      icons::phone(c, 52, 285, 14, ui_c::ICON, p ? ui_c::BTN_PRESSED : ui_c::SCREEN_BG);
      c.text("Set up from your phone", 64, 285, F_UI9, ui_c::ICON, lgfx::middle_left);
      c.text("Changes save automatically", 120, 310, F_UI8, ui_c::TEXT_FAINT, lgfx::middle_center);
    }
  }

  void onTap(int x, int y) override {
    if (headerBackHit(x, y)) return go(SCR_RADAR);
    int row = (y - HEADER_H) / RH;
    if (y >= HEADER_H && row < 6) {
      switch (row) {
        case 0: return openLocation(false);
        case 1: return go(SCR_WIFI);
        case 2: return go(SCR_RADAR_PREFS);
        case 3: return go(SCR_DISPLAY_PREFS);
        case 4: return go(SCR_MILITARY);
        case 5: return go(SCR_ABOUT);
      }
    }
    if (hit(x, y, 30, 267, 180, 36)) go(SCR_SETUP);
  }
};

// =========================================================================
class RadarPrefsScreen : public Screen {
 public:
  void leave() override { settingsSave(); }

  void render(Canvas& c) override {
    c.fillRect(0, c.oy, 240, cfg::BAND_H, ui_c::SCREEN_BG);
    header(c, "Radar");
    char buf[32];

    // 1. details popup stepper
    if (c.visible(36, 31)) {
      rowBg(c, 36, 31);
      c.text("Details popup", 8, 46, F_UI11, ui_c::TEXT, lgfx::middle_left);
      c.text("How long a tapped plane shows", 8, 58, F_UI8, ui_c::TEXT_DIM, lgfx::middle_left);
      button(c, 160, 41, 22, 22, "", F_UI11, ui_c::KEY_BG, ui_c::TEXT, 5);
      icons::minus(c, 171, 52, 9, ui_c::TEXT);
      snprintf(buf, sizeof(buf), "%d s", POPUP_SECONDS[g_set.popupIdx]);
      c.text(buf, 197, 52, F_MONO11, ui_c::TEXT, lgfx::middle_center);
      button(c, 212, 41, 22, 22, "", F_UI11, ui_c::KEY_BG, ui_c::TEXT, 5);
      icons::plus(c, 223, 52, 9, ui_c::TEXT);
    }
    // 2. start-up range
    if (c.visible(67, 46)) {
      rowBg(c, 67, 46);
      snprintf(buf, sizeof(buf), "Start-up range (%s)", distUnit(g_set.units));
      c.text(buf, 8, 77, F_UI11, ui_c::TEXT, lgfx::middle_left);
      static char lab[RANGE_COUNT][6];
      static const char* labels[RANGE_COUNT];
      for (int i = 0; i < RANGE_COUNT; ++i) {
        snprintf(lab[i], sizeof(lab[i]), "%d", rangeValue(g_set.units, i));
        labels[i] = lab[i];
      }
      segmented(c, 8, 87, 224, 21, labels, RANGE_COUNT, g_set.rangeIdx, F_MONO8);
    }
    // 3. labels chips
    if (c.visible(113, 46)) {
      rowBg(c, 113, 46);
      c.text("Labels next to planes", 8, 123, F_UI11, ui_c::TEXT, lgfx::middle_left);
      static const char* names[] = {"Callsign", "Type", "Altitude", "Speed"};
      for (int i = 0; i < 4; ++i) {
        bool on = g_set.labels & (1 << i);
        int x = 8 + i * 57;
        uint16_t bg = pressedIn(x, 133, 53, 20) ? ui_c::BTN_PRESSED : on ? rgb(18, 59, 74) : ui_c::ROW_BG;
        c.roundRect(x, 133, 53, 20, 10, bg);
        c.s->drawRoundRect(x, 133 - c.oy, 53, 20, 10, on ? ui_c::CYAN : ui_c::TOGGLE_OFF);
        c.text(names[i], x + 26, 143, F_UI8, on ? rgb(207, 243, 255) : ui_c::TEXT_DIM, lgfx::middle_center);
      }
    }
    // 4-6 toggles
    toggleRow(c, 159, 29, "Hide planes on the ground", nullptr, g_set.hideGround);
    toggleRow(c, 188, 29, "Airports & runways", nullptr, g_set.runways);
    toggleRow(c, 217, 29, "Trails (last 60 s)", nullptr, g_set.trails);
    // 7. altitude filter
    if (c.visible(246, 31)) {
      rowBg(c, 246, 31);
      c.text("Altitude", 8, 261, F_UI11, ui_c::TEXT, lgfx::middle_left);
      static const char* alt[] = {"All", "< 10k ft", "> 10k ft"};
      segmented(c, 100, 251, 132, 21, alt, 3, g_set.altFilter, F_UI8);
    }
    // 8. data source
    if (c.visible(277, 29)) {
      bool p = pressedIn(0, 277, 240, 29);
      c.fillRect(0, 277, 240, 29, p ? ui_c::BTN_PRESSED : ui_c::ROW_BG);
      c.text("Data source", 8, 291, F_UI11, ui_c::TEXT, lgfx::middle_left);
      snprintf(buf, sizeof(buf), "%s \xC2\xB7 5 s", sourceName(g_set.source));
      c.text(buf, 220, 291, F_MONO8, ui_c::TEXT_DIM, lgfx::middle_right);
      icons::chevronRight(c, 230, 291, 8, ui_c::CHEVRON);
    }
  }

  void onTap(int x, int y) override {
    if (headerBackHit(x, y)) return go(SCR_SETTINGS);
    if (hit(x, y, 154, 36, 34, 31)) {
      if (g_set.popupIdx > 0) g_set.popupIdx--;
    } else if (hit(x, y, 206, 36, 34, 31)) {
      if (g_set.popupIdx < POPUP_COUNT - 1) g_set.popupIdx++;
    } else if (int i = segmentedHit(x, y, 8, 87, 224, 21, RANGE_COUNT); i >= 0) {
      g_set.rangeIdx = i;
      g_applyStartupRange = true;
    } else if (y >= 128 && y < 159) {
      int i = (x - 8) / 57;
      if (i >= 0 && i < 4) g_set.labels ^= (1 << i);
    } else if (hit(x, y, 0, 159, 240, 29)) {
      g_set.hideGround = !g_set.hideGround;
    } else if (hit(x, y, 0, 188, 240, 29)) {
      g_set.runways = !g_set.runways;
    } else if (hit(x, y, 0, 217, 240, 29)) {
      g_set.trails = !g_set.trails;
    } else if (int a = segmentedHit(x, y, 100, 251, 132, 21, 3); a >= 0) {
      g_set.altFilter = a;
    } else if (hit(x, y, 0, 277, 240, 29)) {
      g_set.source = (g_set.source + 1) % SRC_COUNT;
      net::refreshAdsb();
    }
    invalidate();
  }
};

// =========================================================================
class DisplayPrefsScreen : public Screen {
 public:
  void leave() override { settingsSave(); }

  void render(Canvas& c) override {
    c.fillRect(0, c.oy, 240, cfg::BAND_H, ui_c::SCREEN_BG);
    header(c, "Display, units & sound");
    char buf[40];

    // 1. brightness slider
    if (c.visible(36, 31)) {
      rowBg(c, 36, 31);
      c.text("Brightness", 8, 51, F_UI11, ui_c::TEXT, lgfx::middle_left);
      int x0 = SL_X, w = SL_W;
      int fill = (g_set.brightness - 10) * w / 90;
      c.roundRect(x0, 49, w, 5, 2, ui_c::TOGGLE_OFF);
      c.roundRect(x0, 49, fill, 5, 2, ui_c::CYAN);
      c.smoothCircle(x0 + fill, 51, 7, rgb(255, 255, 255));
      snprintf(buf, sizeof(buf), "%d%%", g_set.brightness);
      c.text(buf, 234, 51, F_MONO8, ui_c::HUD_TEXT, lgfx::middle_right);
    }
    // 2. units
    if (c.visible(67, 46)) {
      rowBg(c, 67, 46);
      c.text("Units", 8, 77, F_UI11, ui_c::TEXT, lgfx::middle_left);
      static const char* hint[] = {"mi \xC2\xB7 ft \xC2\xB7 mph \xC2\xB7 \xC2\xB0""F", "km \xC2\xB7 m \xC2\xB7 km/h \xC2\xB7 \xC2\xB0""C",
                                   "nm \xC2\xB7 ft \xC2\xB7 kt \xC2\xB7 \xC2\xB0""F"};
      c.text(hint[g_set.units], 232, 77, F_MONO8, ui_c::TEXT_DIM, lgfx::middle_right);
      static const char* labels[] = {"Imperial", "Metric", "Aviation"};
      segmented(c, 8, 87, 224, 21, labels, 3, g_set.units, F_UI9);
    }
    // 3. theme
    if (c.visible(113, 46)) {
      rowBg(c, 113, 46);
      c.text("Theme", 8, 123, F_UI11, ui_c::TEXT, lgfx::middle_left);
      static const char* names[] = {"Classic", "Amber", "Holiday"};
      const RadarTheme* th[] = {&THEME_CLASSIC, &THEME_AMBER, &THEME_HOLIDAY};
      for (int i = 0; i < 3; ++i) {
        int x = 8 + i * 76;
        bool on = g_set.theme == i;
        c.roundRect(x, 132, 72, 22, 5, pressedIn(x, 132, 72, 22) ? ui_c::BTN_PRESSED : ui_c::SCREEN_BG);
        c.s->drawRoundRect(x, 132 - c.oy, 72, 22, 5, on ? ui_c::CYAN : ui_c::KEY_BORDER);
        if (on) c.s->drawRoundRect(x + 1, 133 - c.oy, 70, 20, 4, ui_c::CYAN);
        c.smoothCircle(x + 12, 143, 5, th[i]->ring);
        c.smoothCircle(x + 12, 143, 2, th[i]->plane);
        c.text(names[i], x + 21, 143, F_UI9, ui_c::TEXT, lgfx::middle_left);
      }
    }
    // 4. clock
    if (c.visible(159, 31)) {
      rowBg(c, 159, 31);
      c.text("Clock", 8, 174, F_UI11, ui_c::TEXT, lgfx::middle_left);
      static const char* labels[] = {"12 h", "24 h"};
      segmented(c, 142, 164, 90, 21, labels, 2, g_set.clock24 ? 1 : 0, F_UI9);
    }
    if (c.visible(190, 29)) {
      c.fillRect(0, 190, 240, 29, pressedIn(0, 190, 240, 29) ? ui_c::BTN_PRESSED : ui_c::ROW_BG);
      rowDivider(c, 218);
      char sub[48], a[12], b[12];
      if (g_set.nightDim) {
        formatClock(a, sizeof(a), g_set.nightStart);
        formatClock(b, sizeof(b), g_set.nightEnd);
        if (g_set.nightBright == 0) snprintf(sub, sizeof(sub), "%s \xE2\x80\x93 %s, screen off", a, b);
        else snprintf(sub, sizeof(sub), "%s \xE2\x80\x93 %s, %d%% brightness", a, b, g_set.nightBright);
      } else {
        strcpy(sub, "Off");
      }
      c.text("Night dimming", 8, 199, F_UI11, ui_c::TEXT, lgfx::middle_left);
      c.text(sub, 8, 211, F_UI8, ui_c::TEXT_DIM, lgfx::middle_left);
      icons::chevronRight(c, 228, 204, 9, ui_c::CHEVRON);
    }
    toggleRow(c, 219, 29, "Rotate screen 180\xC2\xB0", "Puts the USB port on the other side", g_set.flip);
    toggleRow(c, 248, 29, "Alert chime", "Emergency squawk or plane overhead", g_set.chime);
    toggleRow(c, 277, 29, "Santa tracker", "Sleigh on Dec 24 \xC2\xB7 turn on for a preview", g_set.santa);
  }

  void onDown(int x, int y) override { slide(x, y); }
  void onDrag(int x, int y) override { slide(x, y); }

  void onTap(int x, int y) override {
    if (headerBackHit(x, y)) return go(SCR_SETTINGS);
    if (int u = segmentedHit(x, y, 8, 87, 224, 21, 3); u >= 0) {
      if (u != g_set.units) {
        g_set.units = u;
        net::refreshWeather();
        net::refreshAdsb();
      }
    } else if (y >= 128 && y < 159) {
      int i = (x - 8) / 76;
      if (i >= 0 && i < 3) g_set.theme = i;
    } else if (int k = segmentedHit(x, y, 142, 164, 90, 21, 2); k >= 0) {
      g_set.clock24 = k == 1;
    } else if (hit(x, y, 0, 190, 240, 29)) {
      return go(SCR_NIGHT);
    } else if (hit(x, y, 0, 219, 240, 29)) {
      g_set.flip = !g_set.flip;
      applyDisplaySettings();
    } else if (hit(x, y, 0, 248, 240, 29)) {
      g_set.chime = !g_set.chime;
      if (g_set.chime) audio::play(audio::CHIME_TEST);
    } else if (hit(x, y, 0, 277, 240, 29)) {
      g_set.santa = !g_set.santa;
      if (g_set.santa) g_santaPreviewUntil = millis() + 20000;
    }
    invalidate();
  }

 private:
  static constexpr int SL_X = 82, SL_W = 112;
  void slide(int x, int y) {
    if (!hit(x, y, SL_X - 12, 36, SL_W + 24, 31)) return;
    int v = 10 + (x - SL_X) * 90 / SL_W;
    v = constrain(v, 10, 100);
    v = (v + 2) / 5 * 5;
    if (v != g_set.brightness) {
      g_set.brightness = v;
      applyDisplaySettings();
    }
  }
};

// =========================================================================
class AboutScreen : public Screen {
 public:
  void enter() override {
    confirmUntil_ = 0;
    float v;
    batt_ = board::readBattery(v) ? v : -1;
  }

  void tick(uint32_t now) override {
    if (now - last_ > 2000) {
      last_ = now;
      invalidate();
    }
  }

  void render(Canvas& c) override {
    c.fillRect(0, c.oy, 240, cfg::BAND_H, ui_c::SCREEN_BG);
    header(c, "About & reset");
    char buf[48];
    int y = 46;
    auto line = [&](const char* k, const char* v) {
      if (c.visible(y - 6, 13)) {
        c.text(k, 10, y, F_UI9, ui_c::TEXT_DIM, lgfx::middle_left);
        char tmp[48];
        strncpy(tmp, v, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = 0;
        fitText(c, tmp, sizeof(tmp), F_UI9, 140);
        c.text(tmp, 230, y, F_UI9, ui_c::TEXT, lgfx::middle_right);
      }
      y += 15;
    };
    line("Firmware", "v" FW_VERSION);
    if (g_net.wifi == W_CONNECTED) snprintf(buf, sizeof(buf), "%s (%d dBm)", g_set.ssid, g_net.rssi);
    else snprintf(buf, sizeof(buf), "%s", g_set.ssid[0] ? "not connected" : "not set");
    line("Wi-Fi", buf);
    line("IP address", g_net.ip[0] ? g_net.ip : "\xE2\x80\x94");
    if (g_set.locValid) snprintf(buf, sizeof(buf), "%.4f, %.4f", g_set.lat, g_set.lon);
    else strcpy(buf, "not set");
    line("Location", buf);
    if (batt_ > 0) snprintf(buf, sizeof(buf), "%.2f V", batt_);
    else strcpy(buf, "\xE2\x80\x94");
    line("Battery", buf);
    {
      Weather w;
      {
        Lock l;
        w = g_weather;
      }
      if (w.loading) strcpy(buf, "loading...");
      else if (w.fails && !w.valid) snprintf(buf, sizeof(buf), "not answering (%d)", w.lastHttp);
      else if (w.valid) {
        uint32_t mins = (millis() - w.fetchedMs) / 60000;
        if (w.fails) snprintf(buf, sizeof(buf), "%lu min old, retrying", (unsigned long)mins);
        else snprintf(buf, sizeof(buf), "updated %lu min ago", (unsigned long)mins);
      } else strcpy(buf, "\xE2\x80\x94");
      line("Weather", buf);
    }
    snprintf(buf, sizeof(buf), "%u KB free", (unsigned)(ESP.getFreeHeap() / 1024));
    line("Memory", buf);
    uint32_t up = millis() / 1000;
    snprintf(buf, sizeof(buf), "%lud %luh %lum", (unsigned long)(up / 86400), (unsigned long)(up / 3600 % 24),
             (unsigned long)(up / 60 % 60));
    line("Uptime", buf);

    if (c.visible(162, 110)) {
      c.hline(10, 165, 220, ui_c::DIVIDER);
      int cy = 177;
      const char* credits[] = {
        "Aircraft: adsb.fi \xC2\xB7 airplanes.live \xC2\xB7 adsb.lol",
        "Weather: Open-Meteo.com (CC BY 4.0)",
        "Photos: planespotters.net \xC2\xB7 airport-data.com",
        "Aircraft & routes: adsbdb.com",
        "ZIP lookup: zippopotam.us \xC2\xB7 Runways: OurAirports",
        "Fonts: Inter, JetBrains Mono (OFL)",
      };
      for (auto t : credits) {
        c.text(t, 10, cy, F_UI8, ui_c::TEXT_FAINT, lgfx::middle_left);
        cy += 13;
      }
    }

    bool confirm = confirmUntil_ && (int32_t)(confirmUntil_ - millis()) > 0;
    button(c, 8, 278, 108, 32, "Restart", F_UI11, ui_c::KEY_BG, ui_c::TEXT, 8);
    button(c, 124, 278, 108, 32, confirm ? "Tap again" : "Factory reset", F_UI11,
           confirm ? rgb(200, 40, 50) : rgb(70, 24, 30), confirm ? rgb(255, 255, 255) : rgb(255, 150, 150), 8);
  }

  void onTap(int x, int y) override {
    if (headerBackHit(x, y)) return go(SCR_SETTINGS);
    if (hit(x, y, 8, 274, 108, 40)) {
      settingsSave();
      delay(100);
      ESP.restart();
    }
    if (hit(x, y, 124, 274, 108, 40)) {
      if (confirmUntil_ && (int32_t)(confirmUntil_ - millis()) > 0) settingsFactoryReset();
      confirmUntil_ = millis() + 4000;
      invalidate();
    }
  }

 private:
  uint32_t confirmUntil_ = 0, last_ = 0;
  float batt_ = -1;
};

SettingsScreen s_settings;
RadarPrefsScreen s_radarPrefs;
DisplayPrefsScreen s_displayPrefs;
AboutScreen s_about;

}  // namespace

Screen* settingsScreen() { return &s_settings; }
Screen* radarPrefsScreen() { return &s_radarPrefs; }
Screen* displayPrefsScreen() { return &s_displayPrefs; }
Screen* aboutScreen() { return &s_about; }

}  // namespace ui
