#include "ui.h"
#include "icons.h"
#include <WiFi.h>
#include "board.h"
#include "config.h"
#include "net.h"
#include "portal.h"
#include "settings.h"
#include "state.h"

namespace ui {
namespace {

lgfx::LGFX_Sprite s_band(&display::lcd);
Screen* s_screens[SCR_COUNT] = {};
ScreenId s_cur = SCR_RADAR;
bool s_dirty = true;
uint32_t s_lastFrame = 0;

// touch
bool s_down = false;
int s_tx = -1, s_ty = -1, s_downX = 0, s_downY = 0;
uint32_t s_downMs = 0, s_lastPoll = 0, s_lastTouchMs = 0;
bool s_moved = false;

// brightness
int s_appliedBrightness = -1;
uint32_t s_lastBrightCheck = 0;
uint32_t s_wakeUntil = 0, s_previewUntil = 0;
bool s_swallow = false;  // touch that only woke the screen

// portal / first run
uint32_t s_portalIdleSince = 0;
uint8_t s_prevWifi = 255;

void renderAll() {
  for (int b = 0; b < cfg::SCREEN_H / cfg::BAND_H; ++b) {
    Canvas c{&s_band, b * cfg::BAND_H};
    s_screens[s_cur]->render(c);
    s_band.pushSprite(&display::lcd, 0, b * cfg::BAND_H);
  }
}

int effectiveBrightness() {
  uint32_t now = millis();
  if ((int32_t)(s_previewUntil - now) > 0) return g_set.nightBright;
  int b = g_set.brightness;
  if (isNightNow() && (int32_t)(s_wakeUntil - now) <= 0) b = std::min<int>(b, g_set.nightBright);
  return b;
}

void applyBrightnessNow() {
  int b = effectiveBrightness();
  if (b != s_appliedBrightness) {
    s_appliedBrightness = b;
    board::setBacklight(b);
  }
}

void pollTouch(uint32_t now) {
  if (now - s_lastPoll < cfg::TOUCH_POLL_MS) return;
  s_lastPoll = now;
  int16_t x, y;
  bool down = board::readTouch(x, y);
  if (down && g_set.flip) {
    x = cfg::SCREEN_W - 1 - x;
    y = cfg::SCREEN_H - 1 - y;
  }
  if (down) {
    s_lastTouchMs = now;
    if (!s_down) {
      // At night, a touch brightens the screen for 30 s. If the screen was
      // fully off, that first touch only wakes it.
      if (g_set.tapWake && isNightNow()) {
        bool wasOff = s_appliedBrightness == 0;
        s_wakeUntil = now + 30000;
        applyBrightnessNow();
        if (wasOff) {
          s_down = true;
          s_swallow = true;
          s_moved = true;
          s_lastTouchMs = now;
          return;
        }
      }
      s_down = true;
      s_swallow = false;
      s_moved = false;
      s_downX = s_tx = x;
      s_downY = s_ty = y;
      s_downMs = now;
      s_screens[s_cur]->onDown(x, y);
      s_dirty = true;
    } else if (s_swallow) {
      return;
    } else if (abs(x - s_tx) > 1 || abs(y - s_ty) > 1) {
      s_tx = x;
      s_ty = y;
      if (abs(x - s_downX) > 10 || abs(y - s_downY) > 10) s_moved = true;
      s_screens[s_cur]->onDrag(x, y);
    }
  } else if (s_down) {
    // released (allow 2 missed polls before treating as lift-off)
    if (now - s_lastTouchMs < cfg::TOUCH_POLL_MS * 3) return;
    s_down = false;
    if (g_set.tapWake && isNightNow()) s_wakeUntil = now + 30000;
    if (s_swallow) {
      s_swallow = false;
      return;
    }
    int tx = s_tx, ty = s_ty;
    s_tx = s_ty = -1;
    s_dirty = true;
    if (!s_moved && now - s_downMs < 1500) s_screens[s_cur]->onTap(tx, ty);
  }
}

void handlePortalAndSetup(uint32_t now) {
  portal::loop();

  portal::Submission sub;
  if (portal::take(sub)) {
    strncpy(g_set.ssid, sub.ssid, sizeof(g_set.ssid) - 1);
    strncpy(g_set.pass, sub.pass, sizeof(g_set.pass) - 1);
    if (sub.hasLatLon) {
      g_set.locMode = 1;
      g_set.lat = sub.lat;
      g_set.lon = sub.lon;
      g_set.locValid = true;
      g_set.place[0] = 0;
    } else if (sub.zip[0]) {
      g_set.locMode = 0;
      strncpy(g_set.zip, sub.zip, sizeof(g_set.zip) - 1);
      net::requestGeocode(g_set.zip);
    }
    settingsSave();
    net::reconnect();
    if (s_cur == SCR_SETUP || s_cur == SCR_WIFI || s_cur == SCR_WIFI_JOIN) go(SCR_RADAR);
  }

  // ZIP lookup finished?
  uint8_t geo;
  {
    Lock l;
    geo = g_net.geo;
    if (geo == G_OK) {
      g_set.lat = g_net.geoLat;
      g_set.lon = g_net.geoLon;
      strncpy(g_set.place, g_net.geoPlace, sizeof(g_set.place) - 1);
      g_set.locValid = true;
      g_net.geo = G_IDLE;
    }
  }
  if (geo == G_OK) {
    settingsSave();
    net::refreshWeather();
    net::refreshAdsb();
    s_dirty = true;
  }

  uint8_t wifi = g_net.wifi;
  if (wifi != s_prevWifi) {
    if (wifi == W_CONNECTED && s_prevWifi != W_CONNECTED && !g_set.locValid && g_net.geo != G_PENDING &&
        s_cur != SCR_LOCATION) {
      openLocation(true);  // first run: ask where to look
    }
    s_prevWifi = wifi;
    s_dirty = true;
  }

  // Close the setup hotspot once we're online and the setup screen is gone.
  if (portal::active()) {
    if (wifi == W_CONNECTED && s_cur != SCR_SETUP) {
      if (!s_portalIdleSince) s_portalIdleSince = now;
      if (now - s_portalIdleSince > 20000) portal::stop();
    } else {
      s_portalIdleSince = 0;
    }
  }
}

}  // namespace

void begin() {
  s_band.setColorDepth(16);
  s_band.createSprite(cfg::SCREEN_W, cfg::BAND_H);
  s_screens[SCR_RADAR] = radarScreen();
  s_screens[SCR_SETTINGS] = settingsScreen();
  s_screens[SCR_LOCATION] = locationScreen();
  s_screens[SCR_WIFI] = wifiScreen();
  s_screens[SCR_WIFI_JOIN] = wifiJoinScreen();
  s_screens[SCR_RADAR_PREFS] = radarPrefsScreen();
  s_screens[SCR_DISPLAY_PREFS] = displayPrefsScreen();
  s_screens[SCR_ABOUT] = aboutScreen();
  s_screens[SCR_SETUP] = setupScreen();
  s_screens[SCR_NIGHT] = nightScreen();
  s_screens[SCR_MILITARY] = militaryScreen();
  s_cur = SCR_RADAR;
  s_screens[s_cur]->enter();
  if (!g_set.ssid[0]) go(SCR_SETUP);
  renderAll();
  applyDisplaySettings();
}

void loop() {
  uint32_t now = millis();
  pollTouch(now);
  handlePortalAndSetup(now);
  s_screens[s_cur]->tick(now);

  bool frameDue = s_screens[s_cur]->animated() && now - s_lastFrame >= cfg::RADAR_FRAME_MS;
  if (s_dirty || frameDue) {
    s_dirty = false;
    s_lastFrame = now;
    renderAll();
  }

  if (now - s_lastBrightCheck > 250) {
    s_lastBrightCheck = now;
    applyBrightnessNow();
  }
}

void go(ScreenId id) {
  if (id >= SCR_COUNT) return;
  s_screens[s_cur]->leave();
  s_cur = id;
  net::pauseRetries(id == SCR_WIFI || id == SCR_WIFI_JOIN || id == SCR_SETUP);
  s_screens[s_cur]->enter();
  s_dirty = true;
}

ScreenId current() { return s_cur; }
void invalidate() { s_dirty = true; }

void applyDisplaySettings() {
  display::setFlip(g_set.flip);
  s_appliedBrightness = effectiveBrightness();
  board::setBacklight(s_appliedBrightness);
  s_dirty = true;
}

bool isNightNow() {
  if (!g_set.nightDim || g_set.nightStart == g_set.nightEnd) return false;
  struct tm t;
  if (!net::localTime(t)) return false;
  int m = t.tm_hour * 60 + t.tm_min;
  if (g_set.nightStart < g_set.nightEnd) return m >= g_set.nightStart && m < g_set.nightEnd;
  return m >= g_set.nightStart || m < g_set.nightEnd;  // overnight window
}

void previewNight(uint32_t ms) {
  s_previewUntil = millis() + ms;
  applyBrightnessNow();
}

void formatClock(char* out, size_t cap, int minutes) {
  int h = minutes / 60, m = minutes % 60;
  if (g_set.clock24) snprintf(out, cap, "%02d:%02d", h, m);
  else snprintf(out, cap, "%d:%02d %s", h % 12 == 0 ? 12 : h % 12, m, h < 12 ? "AM" : "PM");
}

bool hit(int x, int y, int rx, int ry, int rw, int rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

bool pressedIn(int rx, int ry, int rw, int rh) {
  return s_down && !s_moved && hit(s_tx, s_ty, rx, ry, rw, rh);
}

// ---------------------------------------------------------------------------
// Widgets
// ---------------------------------------------------------------------------
void header(Canvas& c, const char* title, const char* pill, bool backArrow) {
  if (!c.visible(0, HEADER_H)) return;
  c.fillRect(0, 0, cfg::SCREEN_W, HEADER_H, ui_c::HEADER_BG);
  c.hline(0, HEADER_H - 1, cfg::SCREEN_W, ui_c::BORDER);
  int tx = 12;
  if (backArrow) {
    if (pressedIn(0, 0, 36, HEADER_H)) c.fillRect(0, 0, 36, HEADER_H - 1, ui_c::BTN_PRESSED);
    icons::chevronLeft(c, 18, 18, 13, ui_c::HUD_TEXT);
    tx = 36;
  }
  int maxW = cfg::SCREEN_W - tx - (pill ? 64 : 8);
  char buf[48];
  strncpy(buf, title, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;
  fitText(c, buf, sizeof(buf), F_UI13, maxW);
  c.text(buf, tx, 18, F_UI13, ui_c::TEXT, lgfx::middle_left);
  if (pill) {
    bool p = pressedIn(cfg::SCREEN_W - 64, 0, 64, HEADER_H);
    int w = c.textWidth(pill, F_UI11) + 22;
    int x = cfg::SCREEN_W - 7 - w;
    c.roundRect(x, 6, w, 24, 12, p ? rgb(120, 240, 170) : ui_c::GREEN);
    c.text(pill, x + w / 2, 18, F_UI11, ui_c::GREEN_DARK, lgfx::middle_center);
  }
}

bool headerBackHit(int x, int y) { return hit(x, y, 0, 0, 44, HEADER_H); }
bool headerPillHit(int x, int y) { return hit(x, y, cfg::SCREEN_W - 70, 0, 70, HEADER_H); }

void rowDivider(Canvas& c, int y) { c.hline(0, y, cfg::SCREEN_W, ui_c::DIVIDER); }

void navRow(Canvas& c, int y, int h, IconFn icon, const char* label, const char* sub, bool chevron) {
  if (!c.visible(y, h)) return;
  bool p = pressedIn(0, y, cfg::SCREEN_W, h);
  c.fillRect(0, y, cfg::SCREEN_W, h, p ? ui_c::BTN_PRESSED : ui_c::ROW_BG);
  rowDivider(c, y + h - 1);
  int cy = y + h / 2;
  c.roundRect(8, cy - 11, 22, 22, 5, ui_c::ICON_BOX);
  if (icon) icon(c, 19, cy);
  if (sub && *sub) {
    c.text(label, 37, cy - 6, F_UI11, ui_c::TEXT, lgfx::middle_left);
    char buf[48];
    strncpy(buf, sub, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    fitText(c, buf, sizeof(buf), F_UI9, cfg::SCREEN_W - 37 - 20);
    c.text(buf, 37, cy + 7, F_UI9, ui_c::TEXT_DIM, lgfx::middle_left);
  } else {
    c.text(label, 37, cy, F_UI11, ui_c::TEXT, lgfx::middle_left);
  }
  if (chevron) icons::chevronRight(c, cfg::SCREEN_W - 12, cy, 10, ui_c::CHEVRON);
}

void toggle(Canvas& c, int x, int y, bool on) {
  c.roundRect(x, y, 30, 17, 8, on ? ui_c::TOGGLE_ON : ui_c::TOGGLE_OFF);
  c.smoothCircle(on ? x + 21 : x + 8, y + 8, 7, rgb(255, 255, 255));
}

void segmented(Canvas& c, int x, int y, int w, int h, const char* const* labels, int n, int sel, FontId f) {
  if (!c.visible(y, h)) return;
  c.roundRect(x, y, w, h, 5, ui_c::SCREEN_BG);
  c.roundFrame(x, y, w, h, 5, ui_c::BORDER);
  float segW = (w - 4) / (float)n;
  for (int i = 0; i < n; ++i) {
    int sx = x + 2 + (int)(i * segW), sw = (int)((i + 1) * segW) - (int)(i * segW) - 1;
    if (i == sel) c.roundRect(sx, y + 2, sw, h - 4, 4, ui_c::SEG_ON);
    else if (pressedIn(sx, y, sw, h)) c.roundRect(sx, y + 2, sw, h - 4, 4, ui_c::BTN_BG);
    c.text(labels[i], sx + sw / 2, y + h / 2, f, i == sel ? ui_c::TEXT : ui_c::TEXT_DIM, lgfx::middle_center);
  }
}

int segmentedHit(int tx, int ty, int x, int y, int w, int h, int n) {
  if (!hit(tx, ty, x, y - 4, w, h + 8)) return -1;
  int i = (tx - x - 2) * n / (w - 4);
  return i < 0 ? 0 : i >= n ? n - 1 : i;
}

void button(Canvas& c, int x, int y, int w, int h, const char* label, FontId f, uint16_t bg,
            uint16_t fg, int radius) {
  if (!c.visible(y, h)) return;
  bool p = pressedIn(x, y, w, h);
  c.roundRect(x, y, w, h, radius, p ? ui_c::BTN_PRESSED : bg);
  c.text(label, x + w / 2, y + h / 2, f, fg, lgfx::middle_center);
}

}  // namespace ui
