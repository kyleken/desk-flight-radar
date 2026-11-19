// Wi-Fi network list, password keyboard, and phone setup (QR code)
#include <WiFi.h>
#include "icons.h"
#include "net.h"
#include "portal.h"
#include "settings.h"
#include "state.h"
#include "ui.h"

namespace ui {
namespace {

struct Net {
  char ssid[33];
  int8_t rssi;
  bool open;
};

int bars(int rssi) { return rssi > -55 ? 4 : rssi > -67 ? 3 : rssi > -78 ? 2 : 1; }

// =========================================================================
class WifiScreen : public Screen {
 public:
  void enter() override {
    page_ = 0;
    retries_ = 0;
    retryAt_ = 0;
    if (!scanning_) startScan();
  }

  void tick(uint32_t now) override {
    if (scanning_) {
      int n = WiFi.scanComplete();
      if (n >= 0) {
        collect(n);
        WiFi.scanDelete();
        scanning_ = false;
        retryAt_ = 0;
        invalidate();
      } else if (n == WIFI_SCAN_FAILED && now - scanStart_ > 1000) {
        scanning_ = false;
        retryAt_ = now + 1500;
      } else if (now - scanStart_ > 15000) {  // never finished
        scanning_ = false;
        retryAt_ = now + 500;
      }
    } else if (retryAt_ && (int32_t)(now - retryAt_) >= 0) {
      // A scan can't run while a connection attempt is in progress; keep trying.
      retryAt_ = 0;
      if (retries_++ < 20) startScan();
      invalidate();
    }
    if (now - lastRefresh_ > 1000) {  // status card follows the connection
      lastRefresh_ = now;
      invalidate();
    }
  }

  void render(Canvas& c) override {
    c.fillRect(0, c.oy, 240, cfg::BAND_H, ui_c::SCREEN_BG);
    header(c, "Wi-Fi");
    if (c.visible(0, 36)) {
      bool p = pressedIn(200, 0, 40, 36);
      if (p) c.fillRect(200, 0, 40, 35, ui_c::BTN_PRESSED);
      icons::refresh(c, 220, 18, 16, ui_c::HUD_TEXT, p ? ui_c::BTN_PRESSED : ui_c::HEADER_BG);
    }

    // status card
    if (c.visible(42, 40)) {
      uint8_t w = g_net.wifi;
      uint16_t bg = w == W_CONNECTED ? rgb(15, 42, 34) : ui_c::ROW_BG;
      uint16_t bd = w == W_CONNECTED ? rgb(31, 90, 69) : ui_c::BORDER;
      c.roundRect(8, 42, 224, 38, 6, bg);
      c.s->drawRoundRect(8, 42 - c.oy, 224, 38, 6, bd);
      char l1[64], l2[48];
      uint16_t ic;
      if (w == W_CONNECTED) {
        snprintf(l1, sizeof(l1), "Connected to %s", g_set.ssid);
        snprintf(l2, sizeof(l2), "%s \xC2\xB7 %d dBm", g_net.ip, g_net.rssi);
        ic = ui_c::GREEN;
      } else if (w == W_CONNECTING) {
        snprintf(l1, sizeof(l1), "Connecting to %s...", g_set.ssid);
        strcpy(l2, "This takes a few seconds");
        ic = ui_c::AMBER;
      } else if (w == W_FAILED) {
        uint8_t why = g_net.failReason;
        if (why == FAIL_PASSWORD) {
          snprintf(l1, sizeof(l1), "Wrong password for %s", g_set.ssid);
          strcpy(l2, "Tap the network below to retype it");
        } else if (why == FAIL_NOT_FOUND) {
          snprintf(l1, sizeof(l1), "Can't find %s", g_set.ssid);
          strcpy(l2, "Out of range, or 5 GHz only?");
        } else {
          snprintf(l1, sizeof(l1), "Can't connect to %s", g_set.ssid);
          strcpy(l2, "Tap the network to try again");
        }
        ic = ui_c::RED;
      } else {
        strcpy(l1, "Not connected");
        strcpy(l2, "Pick your network below");
        ic = ui_c::TEXT_DIM;
      }
      c.ring(24, 61, 8, 1.6f, ic);
      if (w == W_CONNECTED) icons::check(c, 24, 61, 8, ic);
      else c.smoothCircle(24, 61, 3, ic);
      fitText(c, l1, sizeof(l1), F_UI11, 190);
      c.text(l1, 38, 55, F_UI11, ui_c::TEXT, lgfx::middle_left);
      c.text(l2, 38, 69, F_MONO8, ui_c::TEXT_DIM, lgfx::middle_left);
    }

    // list header
    if (c.visible(86, 12)) {
      c.text("NETWORKS NEARBY \xC2\xB7 2.4 GHz", 8, 92, F_UI8, ui_c::TEXT_DIM, lgfx::middle_left);
      if (scanning_ || retryAt_) c.text("Scanning...", 232, 92, F_UI8, ui_c::TEXT_DIM, lgfx::middle_right);
      else if (count_ > ROWS) {
        char m[16];
        snprintf(m, sizeof(m), "More (%d/%d)", page_ + 1, (count_ + ROWS - 1) / ROWS);
        c.text(m, 232, 92, F_UI8, ui_c::ICON, lgfx::middle_right);
      }
    }
    for (int r = 0; r < ROWS; ++r) {
      int i = page_ * ROWS + r;
      int y = LIST_Y + r * ROW_H;
      if (!c.visible(y, ROW_H)) continue;
      if (i >= count_) {
        if (r == 0 && !scanning_ && !retryAt_) c.text("No networks found \xE2\x80\x94 tap refresh", 120, y + 17, F_UI9, ui_c::TEXT_DIM, lgfx::middle_center);
        continue;
      }
      const Net& n = nets_[i];
      bool cur = g_net.wifi == W_CONNECTED && strcmp(n.ssid, g_set.ssid) == 0;
      c.fillRect(0, y, 240, ROW_H, pressedIn(0, y, 240, ROW_H) ? ui_c::BTN_PRESSED : ui_c::ROW_BG);
      rowDivider(c, y + ROW_H - 1);
      icons::wifi(c, 20, y + 23, 16, bars(n.rssi), ui_c::HUD_TEXT, ui_c::TOGGLE_OFF);
      char name[33];
      strncpy(name, n.ssid, sizeof(name));
      fitText(c, name, sizeof(name), F_UI11, 150);
      c.text(name, 38, y + 12, F_UI11, ui_c::TEXT, lgfx::middle_left);
      c.text(cur ? "Connected" : n.open ? "Open" : "Secured", 38, y + 25, F_UI8, ui_c::TEXT_DIM, lgfx::middle_left);
      if (cur) icons::check(c, 222, y + 17, 10, ui_c::GREEN);
      else if (!n.open) icons::lock(c, 222, y + 17, 14, ui_c::TEXT_DIM, ui_c::ROW_BG);
    }

    button(c, 8, 282, 108, 32, "Hidden network", F_UI9, ui_c::KEY_BG, ui_c::TEXT, 8);
    button(c, 124, 282, 108, 32, "Use your phone", F_UI9, ui_c::KEY_BG, ui_c::ICON, 8);
  }

  void onTap(int x, int y) override {
    if (headerBackHit(x, y)) return go(g_set.ssid[0] ? SCR_SETTINGS : SCR_SETUP);
    if (hit(x, y, 196, 0, 44, 36)) {
      retries_ = 0;
      if (!scanning_) startScan();
      return invalidate();
    }
    if (hit(x, y, 150, 84, 90, 14) && count_ > ROWS) {
      page_ = (page_ + 1) % ((count_ + ROWS - 1) / ROWS);
      return invalidate();
    }
    if (hit(x, y, 8, 278, 108, 40)) return openJoin("", true);
    if (hit(x, y, 124, 278, 108, 40)) return go(SCR_SETUP);
    if (y >= LIST_Y && y < LIST_Y + ROWS * ROW_H) {
      int i = page_ * ROWS + (y - LIST_Y) / ROW_H;
      if (i >= count_) return;
      const Net& n = nets_[i];
      if (n.open) {
        strncpy(g_set.ssid, n.ssid, sizeof(g_set.ssid) - 1);
        g_set.pass[0] = 0;
        settingsSave();
        net::reconnect();
        invalidate();
      } else {
        openJoin(n.ssid, false);
      }
    }
  }

 private:
  static constexpr int ROWS = 5, ROW_H = 34, LIST_Y = 100;

  void startScan() {
    WiFi.scanDelete();
    if (WiFi.scanNetworks(true) == WIFI_SCAN_FAILED) {
      scanning_ = false;
      retryAt_ = millis() + 1500;
      return;
    }
    scanning_ = true;
    scanStart_ = millis();
  }

  void collect(int n) {
    count_ = 0;
    for (int i = 0; i < n; ++i) {
      String s = WiFi.SSID(i);
      if (!s.length()) continue;
      int8_t rssi = WiFi.RSSI(i);
      int found = -1;
      for (int k = 0; k < count_; ++k) {
        if (strcmp(nets_[k].ssid, s.c_str()) == 0) found = k;
      }
      if (found >= 0) {
        if (rssi > nets_[found].rssi) nets_[found].rssi = rssi;
        continue;
      }
      if (count_ >= MAXN) break;
      Net& e = nets_[count_++];
      strncpy(e.ssid, s.c_str(), sizeof(e.ssid) - 1);
      e.ssid[sizeof(e.ssid) - 1] = 0;
      e.rssi = rssi;
      e.open = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
    }
    std::sort(nets_, nets_ + count_, [](const Net& a, const Net& b) { return a.rssi > b.rssi; });
    page_ = 0;
  }

  static constexpr int MAXN = 20;
  Net nets_[MAXN];
  int count_ = 0, page_ = 0, retries_ = 0;
  bool scanning_ = false;
  uint32_t retryAt_ = 0;
  uint32_t scanStart_ = 0, lastRefresh_ = 0;
};

// =========================================================================
class WifiJoinScreen : public Screen {
 public:
  char ssid[33] = "";
  bool hidden = false;

  void enter() override {
    pass_[0] = 0;
    if (!hidden && strcmp(ssid, g_set.ssid) == 0) strncpy(pass_, g_set.pass, sizeof(pass_) - 1);
    shift_ = nums_ = show_ = false;
    field_ = hidden ? 0 : 1;
  }

  void render(Canvas& c) override {
    c.fillRect(0, c.oy, 240, cfg::BAND_H, ui_c::SCREEN_BG);
    header(c, hidden ? "Hidden network" : ssid, "Join");
    if (hidden) {
      field(c, 42, "Name", ssid, field_ == 0, false);
      field(c, 78, "Pass", pass_, field_ == 1, true);
    } else {
      field(c, 42, "Pass", pass_, true, true);
      // phone helper
      if (c.visible(80, 40)) {
        bool p = pressedIn(8, 80, 224, 38);
        c.roundRect(8, 80, 224, 38, 6, p ? ui_c::BTN_PRESSED : ui_c::ROW_BG);
        c.s->drawRoundRect(8, 80 - c.oy, 224, 38, 6, ui_c::BORDER);
        icons::phone(c, 24, 99, 18, ui_c::ICON, p ? ui_c::BTN_PRESSED : ui_c::ROW_BG);
        c.text("Long password? Use your phone", 38, 93, F_UI9, ui_c::TEXT, lgfx::middle_left);
        c.text("Shows a QR code to scan", 38, 106, F_UI8, ui_c::TEXT_DIM, lgfx::middle_left);
      }
    }
    drawKeyboard(c);
  }

  void onTap(int x, int y) override {
    if (headerBackHit(x, y)) return go(SCR_WIFI);
    if (headerPillHit(x, y)) return join();
    const int passY = hidden ? 78 : 42;
    if (hit(x, y, 192, passY, 40, 32)) {  // eye: show / hide password
      show_ = !show_;
      field_ = 1;
      return invalidate();
    }
    if (hidden) {
      if (hit(x, y, 8, 42, 224, 32)) field_ = 0;
      else if (hit(x, y, 8, 78, 224, 32)) field_ = 1;
    } else if (hit(x, y, 8, 80, 224, 38)) {
      return go(SCR_SETUP);
    }
    keyTap(x, y);
    invalidate();
  }

 private:
  static constexpr int KB_Y = cfg::SCREEN_H - 4 - (4 * 30 + 3 * 4);
  static constexpr int KH = 30, KG = 4;

  void field(Canvas& c, int y, const char* label, const char* value, bool active, bool secret) {
    if (!c.visible(y, 32)) return;
    c.roundRect(8, y, 224, 32, 6, ui_c::ROW_BG);
    c.s->drawRoundRect(8, y - c.oy, 224, 32, 6, active ? ui_c::CYAN : ui_c::KEY_BORDER);
    c.text(label, 16, y + 16, F_UI8, ui_c::TEXT_DIM, lgfx::middle_left);
    char shown[66];
    if (secret && !show_) {
      size_t n = strlen(value);
      for (size_t i = 0; i < n && i < 30; ++i) shown[i] = '*';
      shown[std::min(n, (size_t)30)] = 0;
    } else {
      strncpy(shown, value, sizeof(shown) - 1);
      shown[sizeof(shown) - 1] = 0;
    }
    int maxW = secret ? 140 : 176;
    // keep the end of long text visible
    const char* p = shown;
    while (*p && c.textWidth(p, F_MONO11) > maxW) ++p;
    if (!*value) c.text(secret ? "Password" : "Network name", 46, y + 16, F_UI11, ui_c::TEXT_FAINT, lgfx::middle_left);
    int w = c.text(p, 46, y + 16, F_MONO11, ui_c::TEXT, lgfx::middle_left);
    if (active) c.fillRect(48 + (*value ? w : 0), y + 9, 2, 14, ui_c::CYAN);
    if (secret) {
      icons::eye(c, 214, y + 16, 16, show_ ? ui_c::CYAN : ui_c::TEXT_DIM, ui_c::ROW_BG);
    }
  }

  const char* row(int r) const {
    static const char* L[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
    static const char* S[] = {"1234567890", "@#$%&*-+(", ")!'\":;/?"};
    return nums_ ? S[r] : L[r];
  }

  // Key geometry: rows of up to 10 keys across 232 px
  void keyRect(int r, int i, int n, int& x, int& y, int& w) const {
    float kw = (232 - 9 * 3) / 10.0f;
    y = KB_Y + r * (KH + KG);
    float rowW = n * kw + (n - 1) * 3;
    float x0 = r == 2 ? 4 + kw * 1.5f + 3 : 4 + (232 - rowW) / 2;
    x = (int)(x0 + i * (kw + 3));
    w = (int)kw;
  }

  void key(Canvas& c, int x, int y, int w, const char* label, uint16_t bg, bool shiftIcon = false, bool delIcon = false) {
    bool p = pressedIn(x, y, w, KH);
    uint16_t b = p ? ui_c::BTN_PRESSED : bg;
    c.roundRect(x, y, w, KH, 4, b);
    c.s->drawRoundRect(x, y - c.oy, w, KH, 4, ui_c::KEY_BORDER);
    if (shiftIcon) icons::shift(c, x + w / 2, y + KH / 2, 12, shift_ ? ui_c::CYAN : ui_c::TEXT);
    else if (delIcon) icons::backspace(c, x + w / 2, y + KH / 2, 16, ui_c::TEXT, b);
    else c.text(label, x + w / 2, y + KH / 2, strlen(label) > 1 ? F_UI9 : F_UI11, ui_c::TEXT, lgfx::middle_center);
  }

  void drawKeyboard(Canvas& c) {
    if (c.oy + cfg::BAND_H <= KB_Y) return;
    char s[2] = {0, 0};
    for (int r = 0; r < 3; ++r) {
      const char* k = row(r);
      int n = strlen(k);
      for (int i = 0; i < n; ++i) {
        int x, y, w;
        keyRect(r, i, n, x, y, w);
        s[0] = (shift_ && !nums_) ? toupper(k[i]) : k[i];
        key(c, x, y, w, s, ui_c::KEY_BG);
      }
    }
    int y2 = KB_Y + 2 * (KH + KG), y3 = KB_Y + 3 * (KH + KG);
    float kw = (232 - 9 * 3) / 10.0f;
    key(c, 4, y2, (int)(kw * 1.5f), "", ui_c::KEY_ALT, true);
    key(c, 236 - (int)(kw * 1.5f), y2, (int)(kw * 1.5f), "", ui_c::KEY_ALT, false, true);
    key(c, 4, y3, 44, nums_ ? "ABC" : "123", ui_c::KEY_ALT);
    key(c, 52, y3, 112, "space", ui_c::KEY_BG);
    key(c, 168, y3, 32, ".", ui_c::KEY_BG);
    key(c, 204, y3, 32, nums_ ? "_" : "-", ui_c::KEY_BG);
  }

  void type(char ch) {
    char* buf = (hidden && field_ == 0) ? ssid : pass_;
    size_t cap = (hidden && field_ == 0) ? 33 : 65;
    size_t n = strlen(buf);
    if (n + 1 < cap) {
      buf[n] = ch;
      buf[n + 1] = 0;
    }
    shift_ = false;
  }

  void keyTap(int x, int y) {
    if (y < KB_Y - 2) return;
    for (int r = 0; r < 3; ++r) {
      const char* k = row(r);
      int n = strlen(k);
      for (int i = 0; i < n; ++i) {
        int kx, ky, w;
        keyRect(r, i, n, kx, ky, w);
        if (hit(x, y, kx - 1, ky - 2, w + 3, KH + 4)) {
          type((shift_ && !nums_) ? toupper(k[i]) : k[i]);
          return;
        }
      }
    }
    int y2 = KB_Y + 2 * (KH + KG), y3 = KB_Y + 3 * (KH + KG);
    float kw = (232 - 9 * 3) / 10.0f;
    if (hit(x, y, 0, y2, (int)(kw * 1.5f) + 6, KH)) shift_ = !shift_;
    else if (hit(x, y, 236 - (int)(kw * 1.5f) - 2, y2, (int)(kw * 1.5f) + 6, KH)) {
      char* buf = (hidden && field_ == 0) ? ssid : pass_;
      size_t n = strlen(buf);
      if (n) buf[n - 1] = 0;
    } else if (hit(x, y, 0, y3, 50, KH + 4)) nums_ = !nums_;
    else if (hit(x, y, 50, y3, 116, KH + 4)) type(' ');
    else if (hit(x, y, 166, y3, 36, KH + 4)) type('.');
    else if (hit(x, y, 202, y3, 38, KH + 4)) type(nums_ ? '_' : '-');
  }

  void join() {
    if (!ssid[0]) {
      field_ = 0;
      return invalidate();
    }
    strncpy(g_set.ssid, ssid, sizeof(g_set.ssid) - 1);
    strncpy(g_set.pass, pass_, sizeof(g_set.pass) - 1);
    settingsSave();
    net::reconnect();
    go(SCR_WIFI);
  }

  char pass_[65] = "";
  bool shift_ = false, nums_ = false, show_ = false;
  uint8_t field_ = 1;
};

// =========================================================================
class SetupScreen : public Screen {
 public:
  void enter() override { portal::start(); }

  void tick(uint32_t now) override {
    if (now - last_ > 1000) {
      last_ = now;
      invalidate();
    }
  }

  void render(Canvas& c) override {
    c.fillRect(0, c.oy, 240, cfg::BAND_H, ui_c::SCREEN_BG);
    bool configured = g_set.ssid[0] != 0;
    header(c, configured ? "Set up from your phone" : "Welcome to Flight Radar", nullptr, configured);
    const char* ap = portal::apName();

    if (c.visible(40, 16)) c.text("1. Scan with your phone camera", 120, 48, F_UI11, ui_c::TEXT, lgfx::middle_center);
    // QR code that joins the radar's own setup hotspot
    if (c.visible(58, 128)) {
      c.roundRect(56, 58, 128, 128, 8, rgb(255, 255, 255));
      char qr[64];
      snprintf(qr, sizeof(qr), "WIFI:T:nopass;S:%s;;", ap);
      c.s->qrcode(qr, 64, 66 - c.oy, 112, 3);
    }
    if (c.visible(188, 70)) {
      char line[64];
      snprintf(line, sizeof(line), "or join Wi-Fi \"%s\"", ap);
      c.text(line, 120, 196, F_UI9, ui_c::TEXT_DIM, lgfx::middle_center);
      c.text("2. Fill in the page that opens", 120, 214, F_UI11, ui_c::TEXT, lgfx::middle_center);
      c.text("(or browse to 192.168.4.1)", 120, 229, F_UI9, ui_c::TEXT_DIM, lgfx::middle_center);
    }
    button(c, 20, 250, 200, 30, "Pick a network on this screen", F_UI9, ui_c::KEY_BG, ui_c::TEXT, 15);
    if (c.visible(288, 32)) {
      const char* st = g_net.wifi == W_CONNECTED ? "Online \xE2\x80\x94 you're all set" :
                       g_net.wifi == W_CONNECTING ? "Connecting..." :
                       g_net.wifi == W_FAILED ? (g_net.failReason == FAIL_PASSWORD ? "Wrong password \xE2\x80\x94 please try again" : "Couldn't connect \xE2\x80\x94 try again") : "Waiting for setup...";
      c.text(st, 120, 300, F_UI9, g_net.wifi == W_CONNECTED ? ui_c::GREEN : ui_c::TEXT_DIM, lgfx::middle_center);
    }
  }

  void onTap(int x, int y) override {
    if (g_set.ssid[0] && headerBackHit(x, y)) return go(SCR_SETTINGS);
    if (hit(x, y, 20, 246, 200, 38)) return go(SCR_WIFI);
    if (g_net.wifi == W_CONNECTED && y > 286) return go(SCR_RADAR);
  }

 private:
  uint32_t last_ = 0;
};

WifiScreen s_wifi;
WifiJoinScreen s_join;
SetupScreen s_setup;

}  // namespace

Screen* wifiScreen() { return &s_wifi; }
Screen* wifiJoinScreen() { return &s_join; }
Screen* setupScreen() { return &s_setup; }

void openJoin(const char* ssid, bool hidden) {
  strncpy(s_join.ssid, ssid, sizeof(s_join.ssid) - 1);
  s_join.ssid[sizeof(s_join.ssid) - 1] = 0;
  s_join.hidden = hidden;
  go(SCR_WIFI_JOIN);
}

}  // namespace ui
