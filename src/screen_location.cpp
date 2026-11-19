// Location: US ZIP code or latitude / longitude, with an on-screen keypad
#include "icons.h"
#include "net.h"
#include "settings.h"
#include "state.h"
#include "ui.h"

namespace ui {
namespace {

constexpr int KP_X = 8, KP_W = 53, KP_H = 28, KP_GAP = 4;
constexpr int KP_Y = cfg::SCREEN_H - 6 - (4 * KP_H + 3 * KP_GAP);

struct Key { const char* label; int col, row, span; };
const Key KEYS[] = {
  {"1", 0, 0, 1}, {"2", 1, 0, 1}, {"3", 2, 0, 1}, {"del", 3, 0, 1},
  {"4", 0, 1, 1}, {"5", 1, 1, 1}, {"6", 2, 1, 1}, {"-", 3, 1, 1},
  {"7", 0, 2, 1}, {"8", 1, 2, 1}, {"9", 2, 2, 1}, {".", 3, 2, 1},
  {"0", 0, 3, 2}, {"next", 2, 3, 2},
};

class LocationScreen : public Screen {
 public:
  bool firstRun = false;

  void enter() override {
    mode_ = g_set.locMode;
    strncpy(zip_, g_set.zip, sizeof(zip_));
    if (g_set.locValid) {
      snprintf(lat_, sizeof(lat_), "%.4f", g_set.lat);
      snprintf(lon_, sizeof(lon_), "%.4f", g_set.lon);
    } else {
      lat_[0] = lon_[0] = 0;
    }
    field_ = 0;
    waiting_ = false;
    err_[0] = 0;
  }

  void tick(uint32_t now) override {
    if (now - blink_ > 500) {  // caret blink
      blink_ = now;
      invalidate();
    }
    if (!waiting_) return;
    uint8_t geo;
    {
      Lock l;
      geo = g_net.geo;
      if (geo == G_NOT_FOUND || geo == G_ERROR) g_net.geo = G_IDLE;
    }
    if (geo == G_IDLE && g_set.locValid && strcmp(g_set.zip, zip_) == 0) {
      waiting_ = false;
      finish();
    } else if (geo == G_NOT_FOUND) {
      waiting_ = false;
      strcpy(err_, "ZIP code not found");
      invalidate();
    } else if (geo == G_ERROR) {
      waiting_ = false;
      strcpy(err_, "Lookup failed \xE2\x80\x94 check Wi-Fi");
      invalidate();
    }
  }

  void render(Canvas& c) override {
    c.fillRect(0, c.oy, 240, cfg::BAND_H, ui_c::SCREEN_BG);
    header(c, firstRun ? "Where should I look?" : "Location", "Save", !firstRun);
    static const char* tabs[] = {"ZIP code", "Lat / Long"};
    segmented(c, 8, 44, 224, 26, tabs, 2, mode_, F_UI11);

    int hintY;
    if (mode_ == 0) {
      field(c, 78, "ZIP", zip_, true);
      hintY = 124;
    } else {
      field(c, 78, "LAT", lat_, field_ == 0);
      field(c, 114, "LON", lon_, field_ == 1);
      hintY = 160;
    }

    // hint / status line
    if (c.visible(hintY - 8, 16)) {
      char hint[48];
      uint16_t col = rgb(127, 224, 168);
      if (waiting_) {
        strcpy(hint, "Looking up ZIP code...");
        col = ui_c::TEXT_DIM;
      } else if (err_[0]) {
        strcpy(hint, err_);
        col = ui_c::RED;
      } else if (mode_ == 0) {
        if (strlen(zip_) != 5) {
          strcpy(hint, "Enter a 5-digit US ZIP code");
          col = ui_c::AMBER;
        } else if (g_set.locValid && strcmp(zip_, g_set.zip) == 0 && g_set.place[0]) {
          snprintf(hint, sizeof(hint), "%s \xC2\xB7 %.3f, %.3f", g_set.place, g_set.lat, g_set.lon);
        } else {
          strcpy(hint, "Tap Save to look it up");
        }
      } else {
        double la, lo;
        if (parse(la, lo)) strcpy(hint, "Radar centers on this point");
        else {
          strcpy(hint, "Lat -90..." "90, Long -180..." "180");
          col = ui_c::AMBER;
        }
      }
      icons::pin(c, 15, hintY, 11, col, ui_c::SCREEN_BG);
      c.text(hint, 24, hintY, F_UI9, col, lgfx::middle_left);
    }

    // keypad
    if (c.oy + cfg::BAND_H > KP_Y) {
      for (const Key& k : KEYS) {
        int x = KP_X + k.col * (KP_W + KP_GAP);
        int y = KP_Y + k.row * (KP_H + KP_GAP);
        int w = KP_W * k.span + KP_GAP * (k.span - 1);
        bool disabled = mode_ == 0 && (!strcmp(k.label, "-") || !strcmp(k.label, ".") || !strcmp(k.label, "next"));
        bool alt = !strcmp(k.label, "next") || !strcmp(k.label, "del");
        uint16_t bg = pressedIn(x, y, w, KP_H) && !disabled ? ui_c::BTN_PRESSED : alt ? ui_c::KEY_ALT : ui_c::KEY_BG;
        c.roundRect(x, y, w, KP_H, 5, bg);
        c.s->drawRoundRect(x, y - c.oy, w, KP_H, 5, ui_c::KEY_BORDER);
        uint16_t fg = disabled ? ui_c::TEXT_FAINT : ui_c::TEXT;
        int cx = x + w / 2, cy = y + KP_H / 2;
        if (!strcmp(k.label, "del")) icons::backspace(c, cx, cy, 16, fg, bg);
        else if (!strcmp(k.label, "next")) c.text("Next", cx, cy, F_UI11, fg, lgfx::middle_center);
        else if (!strcmp(k.label, "-")) c.text("\xC2\xB1", cx, cy, F_MONO11, fg, lgfx::middle_center);
        else c.text(k.label, cx, cy, F_MONO11, fg, lgfx::middle_center);
      }
    }
  }

  void onTap(int x, int y) override {
    if (!firstRun && headerBackHit(x, y)) return go(SCR_SETTINGS);
    if (headerPillHit(x, y)) return save();
    if (int t = segmentedHit(x, y, 8, 44, 224, 26, 2); t >= 0) {
      mode_ = t;
      err_[0] = 0;
    } else if (mode_ == 1 && hit(x, y, 8, 78, 224, 32)) {
      field_ = 0;
    } else if (mode_ == 1 && hit(x, y, 8, 114, 224, 32)) {
      field_ = 1;
    } else {
      for (const Key& k : KEYS) {
        int kx = KP_X + k.col * (KP_W + KP_GAP);
        int ky = KP_Y + k.row * (KP_H + KP_GAP);
        int w = KP_W * k.span + KP_GAP * (k.span - 1);
        if (hit(x, y, kx - 2, ky - 2, w + 4, KP_H + 4)) {
          press(k.label);
          break;
        }
      }
    }
    invalidate();
  }

 private:
  void field(Canvas& c, int y, const char* label, const char* value, bool active) {
    if (!c.visible(y, 32)) return;
    c.roundRect(8, y, 224, 32, 6, ui_c::ROW_BG);
    c.s->drawRoundRect(8, y - c.oy, 224, 32, 6, active ? ui_c::CYAN : ui_c::KEY_BORDER);
    if (active) c.s->drawRoundRect(9, y + 1 - c.oy, 222, 30, 5, ui_c::CYAN);
    c.text(label, 18, y + 16, F_UI8, ui_c::TEXT_DIM, lgfx::middle_left);
    int w = c.text(value, 46, y + 16, F_MONO15, ui_c::TEXT, lgfx::middle_left);
    if (active && (millis() / 500) % 2 == 0) c.fillRect(48 + w, y + 8, 2, 16, ui_c::CYAN);
  }

  bool parse(double& la, double& lo) {
    char* e1;
    char* e2;
    la = strtod(lat_, &e1);
    lo = strtod(lon_, &e2);
    return lat_[0] && lon_[0] && *e1 == 0 && *e2 == 0 && fabs(la) <= 90 && fabs(lo) <= 180 && !(la == 0 && lo == 0);
  }

  void press(const char* k) {
    err_[0] = 0;
    char* buf = mode_ == 0 ? zip_ : field_ == 0 ? lat_ : lon_;
    size_t cap = mode_ == 0 ? 6 : 12;
    size_t n = strlen(buf);
    if (!strcmp(k, "del")) {
      if (n) buf[n - 1] = 0;
    } else if (!strcmp(k, "next")) {
      if (mode_ == 1) field_ ^= 1;
    } else if (!strcmp(k, "-")) {
      if (mode_ == 1) {
        if (buf[0] == '-') memmove(buf, buf + 1, n);
        else if (n + 1 < cap) {
          memmove(buf + 1, buf, n + 1);
          buf[0] = '-';
        }
      }
    } else if (!strcmp(k, ".")) {
      if (mode_ == 1 && !strchr(buf, '.') && n + 1 < cap) strcat(buf, ".");
    } else if (n + 1 < cap) {
      buf[n] = k[0];
      buf[n + 1] = 0;
    }
  }

  void save() {
    if (mode_ == 0) {
      if (strlen(zip_) != 5) {
        strcpy(err_, "Enter a 5-digit US ZIP code");
        invalidate();
        return;
      }
      g_set.locMode = 0;
      if (g_set.locValid && strcmp(zip_, g_set.zip) == 0 && g_set.place[0]) {
        settingsSave();
        return finish();
      }
      strncpy(g_set.zip, zip_, sizeof(g_set.zip));
      g_set.locValid = false;
      g_set.place[0] = 0;
      settingsSave();
      if (g_net.wifi != W_CONNECTED) {
        strcpy(err_, "Saved \xE2\x80\x94 will look up once online");
        invalidate();
        net::requestGeocode(zip_);
        return;
      }
      net::requestGeocode(zip_);
      waiting_ = true;
      invalidate();
    } else {
      double la, lo;
      if (!parse(la, lo)) {
        strcpy(err_, "Check the numbers");
        invalidate();
        return;
      }
      g_set.locMode = 1;
      g_set.lat = la;
      g_set.lon = lo;
      g_set.locValid = true;
      g_set.place[0] = 0;
      settingsSave();
      finish();
    }
  }

  void finish() {
    net::refreshWeather();
    net::refreshAdsb();
    go(firstRun ? SCR_RADAR : SCR_SETTINGS);
    firstRun = false;
  }

  uint8_t mode_ = 0, field_ = 0;
  char zip_[6] = "", lat_[12] = "", lon_[12] = "";
  char err_[48] = "";
  bool waiting_ = false;
  uint32_t blink_ = 0;
};

LocationScreen s_location;

}  // namespace

Screen* locationScreen() { return &s_location; }

void openLocation(bool firstRun) {
  s_location.firstRun = firstRun;
  go(SCR_LOCATION);
}

}  // namespace ui
