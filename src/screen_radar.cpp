// Home screen: 240x240 square radar + 240x80 weather / details strip
#include <math.h>
#include "airports.h"
#include "audio.h"
#include "icons.h"
#include "net.h"
#include "settings.h"
#include "state.h"
#include "ui.h"

uint32_t g_santaPreviewUntil = 0;   // set by the settings screen to show a preview
bool g_applyStartupRange = false;  // start-up range changed in settings

namespace ui {
namespace {

constexpr uint32_t SANTA_HEX = 0xFFFFFE;
constexpr float D2R = 0.01745329252f;
constexpr int MAXA = cfg::MAX_AIRCRAFT;

struct Pt {
  float x, y;     // screen
  float nmX, nmY; // offset from home, nautical miles (east, north)
  bool inside, show;
};

struct Trail {
  uint32_t hex;
  uint8_t n;
  int16_t x[cfg::TRAIL_POINTS], y[cfg::TRAIL_POINTS];  // 1/100 nm
};

struct Rwy { float x0, y0, x1, y1; };      // nm
struct ApLabel { char id[5]; float x, y; };  // nm
struct Flake { float x, y, v; };
struct Alerted { uint32_t hex, ms; };

const RadarTheme& theme() {
  switch (g_set.theme) {
    case T_AMBER: return THEME_AMBER;
    case T_HOLIDAY: return THEME_HOLIDAY;
    default: return THEME_CLASSIC;
  }
}

const char* compass8(float deg) {
  static const char* d[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  int i = (int)floorf(fmodf(deg + 360.0f + 22.5f, 360.0f) / 45.0f);
  return d[i & 7];
}

const char* wmoText(int code) {
  switch (code) {
    case 0: return "Clear";
    case 1: return "Mostly clear";
    case 2: return "Partly cloudy";
    case 3: return "Overcast";
    case 45: case 48: return "Fog";
    case 51: case 53: case 55: return "Drizzle";
    case 56: case 57: return "Freezing drizzle";
    case 61: return "Light rain";
    case 63: return "Rain";
    case 65: return "Heavy rain";
    case 66: case 67: return "Freezing rain";
    case 71: return "Light snow";
    case 73: return "Snow";
    case 75: return "Heavy snow";
    case 77: return "Snow grains";
    case 80: case 81: return "Showers";
    case 82: return "Heavy showers";
    case 85: case 86: return "Snow showers";
    case 95: return "Thunderstorm";
    case 96: case 99: return "T-storm, hail";
    default: return "";
  }
}

bool isEmergency(uint16_t sq) { return sq == 7500 || sq == 7600 || sq == 7700; }

class RadarScreen : public Screen {
 public:
  void enter() override {
    if (!inited_) {
      inited_ = true;
      rangeIdx_ = g_set.rangeIdx;
      photo_.setColorDepth(16);
      photo_.createSprite(cfg::PHOTO_W, cfg::PHOTO_H);
      for (auto& f : snow_) {
        f.x = random(0, 240);
        f.y = random(0, 240);
        f.v = 0.6f + random(0, 100) / 80.0f;
      }
    }
    if (g_applyStartupRange) {
      g_applyStartupRange = false;
      rangeIdx_ = g_set.rangeIdx;
    }
    updateFetchRadius();
    rwyRange_ = -1;
  }

  bool animated() override { return true; }

  void tick(uint32_t now) override {
    // New aircraft data?
    bool fresh = false;
    {
      Lock l;
      if (g_adsb.version != acVersion_) {
        acVersion_ = g_adsb.version;
        acCount_ = g_adsb.count;
        memcpy(ac_, g_adsb.list, sizeof(Aircraft) * acCount_);
        fresh = true;
      }
    }
    if (fresh) {
      updateTrails();
      checkAlerts(now);
    }
    // The countdown starts once the details and photo have arrived (max 12 s wait).
    if (selHex_ && selHex_ != SANTA_HEX && now - selStart_ < 12000) {
      bool loading;
      {
        Lock l;
        loading = g_details.hex == selHex_ && (g_details.state != D_DONE || g_details.photoLoading);
      }
      if (loading) selUntil_ = now + POPUP_SECONDS[g_set.popupIdx] * 1000;
    }
    if (selHex_ && (int32_t)(now - selUntil_) > 0) selHex_ = 0;

    if (now - lastSecond_ > 1000) {
      lastSecond_ = now;
      struct tm t;
      bool xmas = net::localTime(t) && t.tm_mon == 11 &&
                  (t.tm_mday == 24 || (t.tm_mday == 25 && t.tm_hour < 6));
      santaOn_ = g_set.santa && (xmas || (int32_t)(g_santaPreviewUntil - now) > 0);
      if (!santaOn_ && selHex_ == SANTA_HEX) selHex_ = 0;
    }
  }

  void render(Canvas& c) override {
    if (c.oy == 0) prepareFrame(millis());
    if (c.oy < cfg::STRIP_Y) drawRadar(c);
    if (c.visible(cfg::STRIP_Y, cfg::STRIP_H)) drawStrip(c);
  }

  void onTap(int x, int y) override {
    uint32_t now = millis();
    if (y >= cfg::STRIP_Y) {
      if (selHex_) {
        selHex_ = 0;
        return;
      }
      if (hit(x, y, 160, 240, 40, 40)) zoom(-1);
      else if (hit(x, y, 160, 280, 40, 40)) zoom(+1);
      else if (hit(x, y, 200, 240, 40, 80)) go(SCR_SETTINGS);
      else if (!g_set.locValid) openLocation(false);
      else net::refreshWeather();  // tap the weather to refresh it now
      return;
    }
    // status chip (bottom-right) jumps to Wi-Fi when offline
    if (g_net.wifi != W_CONNECTED && hit(x, y, 150, 214, 90, 26)) {
      go(SCR_WIFI);
      return;
    }
    if (!g_set.locValid && hit(x, y, 40, 95, 160, 50)) {
      openLocation(false);
      return;
    }
    // nearest aircraft
    int best = -1;
    float bestD = 18 * 18;
    for (int i = 0; i < acCount_; ++i) {
      if (!pts_[i].show) continue;
      float dx = pts_[i].x - x, dy = pts_[i].y - y, d = dx * dx + dy * dy;
      float lim = pts_[i].inside ? 18 * 18 : 14 * 14;
      if (d < lim && d < bestD) {
        bestD = d;
        best = i;
      }
    }
    bool santaHit = false;
    if (santaOn_) {
      float dx = santaX_ - x, dy = santaY_ - y, d = dx * dx + dy * dy;
      if (d < 20 * 20 && d < bestD) santaHit = true;
    }
    uint32_t dur = POPUP_SECONDS[g_set.popupIdx] * 1000;
    if (santaHit) {
      selHex_ = SANTA_HEX;
      selUntil_ = now + dur;
    } else if (best >= 0) {
      selHex_ = ac_[best].hex;
      selUntil_ = now + dur;
      selStart_ = now;
      photoOk_ = false;
      photoSerial_ = 0;
      net::requestDetails(ac_[best]);
    } else if (selHex_) {
      selHex_ = 0;
    }
  }

 private:
  // ---------------------------------------------------------------- data
  void updateFetchRadius() {
    float rangeNm = rangeValue(g_set.units, rangeIdx_) / unitsPerNm(g_set.units);
    net::setFetchRadiusNm(rangeNm * 1.45f + 1.0f);  // reach the square's corners
  }

  void zoom(int dir) {
    int n = rangeIdx_ + dir;
    if (n < 0 || n >= RANGE_COUNT) return;
    rangeIdx_ = n;
    updateFetchRadius();
    net::refreshAdsb();
    rwyRange_ = -1;
  }

  void nmOffset(float lat, float lon, float& x, float& y) const {
    x = (lon - lon0_) * 60.0f * cosLat_;
    y = (lat - lat0_) * 60.0f;
  }

  void updateTrails() {
    static Trail next[MAXA];
    if (!g_set.trails) {
      trailCount_ = 0;
      return;
    }
    lat0_ = g_set.lat;
    lon0_ = g_set.lon;
    cosLat_ = cosf(lat0_ * D2R);
    for (int i = 0; i < acCount_; ++i) {
      Trail* old = nullptr;
      for (int k = 0; k < trailCount_; ++k) {
        if (trails_[k].hex == ac_[i].hex) {
          old = &trails_[k];
          break;
        }
      }
      Trail& t = next[i];
      if (old) t = *old;
      else {
        t.hex = ac_[i].hex;
        t.n = 0;
      }
      float x, y;
      nmOffset(ac_[i].lat, ac_[i].lon, x, y);
      int16_t ix = (int16_t)constrain(x * 100, -32000, 32000), iy = (int16_t)constrain(y * 100, -32000, 32000);
      if (t.n == cfg::TRAIL_POINTS) {
        memmove(t.x, t.x + 1, sizeof(int16_t) * (cfg::TRAIL_POINTS - 1));
        memmove(t.y, t.y + 1, sizeof(int16_t) * (cfg::TRAIL_POINTS - 1));
        t.n--;
      }
      t.x[t.n] = ix;
      t.y[t.n] = iy;
      t.n++;
    }
    memcpy(trails_, next, sizeof(Trail) * acCount_);
    trailCount_ = acCount_;
  }

  void checkAlerts(uint32_t now) {
    if (!g_set.chime || !g_set.locValid) return;
    lat0_ = g_set.lat;
    lon0_ = g_set.lon;
    cosLat_ = cosf(lat0_ * D2R);
    for (int i = 0; i < acCount_; ++i) {
      const Aircraft& a = ac_[i];
      bool emergency = isEmergency(a.squawk);
      float x, y;
      nmOffset(a.lat, a.lon, x, y);
      bool overhead = !a.ground && a.alt != ALT_UNKNOWN && a.alt < 10000 && x * x + y * y < 1.5f * 1.5f;
      if (!emergency && !overhead) continue;
      bool seen = false;
      for (auto& al : alerted_) {
        if (al.hex == a.hex && now - al.ms < 15 * 60 * 1000UL) seen = true;
      }
      if (seen) continue;
      alerted_[alertPos_] = {a.hex, now};
      alertPos_ = (alertPos_ + 1) % (sizeof(alerted_) / sizeof(alerted_[0]));
      audio::play(emergency ? audio::CHIME_EMERGENCY : audio::CHIME_OVERHEAD);
      break;  // one chime per update
    }
  }

  void prepareFrame(uint32_t now) {
    units_ = g_set.units;
    range_ = rangeValue(units_, rangeIdx_);
    pxPerNm_ = cfg::RING_R * unitsPerNm(units_) / range_;
    lat0_ = g_set.lat;
    lon0_ = g_set.lon;
    cosLat_ = cosf(lat0_ * D2R);
    const float lim = cfg::EDGE_LIM;
    count_ = 0;

    for (int i = 0; i < acCount_; ++i) {
      const Aircraft& a = ac_[i];
      Pt& p = pts_[i];
      p.show = false;
      if (!g_set.locValid) continue;
      if (now - a.posMs > cfg::STALE_AFTER_MS) continue;
      if (a.ground && g_set.hideGround) continue;
      if (g_set.altFilter == ALT_LOW && (a.alt == ALT_UNKNOWN || a.alt >= 10000)) continue;
      if (g_set.altFilter == ALT_HIGH && (a.alt == ALT_UNKNOWN || a.alt < 10000)) continue;
      nmOffset(a.lat, a.lon, p.nmX, p.nmY);
      // dead reckoning since the position was reported
      float dt = (now - a.posMs) / 3600000.0f;
      if (dt > 30.0f / 3600) dt = 30.0f / 3600;
      if (!a.ground && a.gs > 30) {
        p.nmX += sinf(a.track * D2R) * a.gs * dt;
        p.nmY += cosf(a.track * D2R) * a.gs * dt;
      }
      float dx = p.nmX * pxPerNm_, dy = -p.nmY * pxPerNm_;
      p.inside = fabsf(dx) <= lim && fabsf(dy) <= lim;
      if (!p.inside) {
        float k = lim / std::max(fabsf(dx), fabsf(dy));
        dx *= k;
        dy *= k;
      } else {
        count_++;
      }
      p.x = cfg::RADAR_C + dx;
      p.y = cfg::RADAR_C + dy;
      p.show = true;
    }

    if (g_set.theme == T_HOLIDAY) {
      for (auto& f : snow_) {
        f.y += f.v;
        f.x += sinf((f.y + f.v * 50) * 0.05f) * 0.4f;
        if (f.y > 240) {
          f.y = -2;
          f.x = random(0, 240);
        }
      }
    }

    if (santaOn_) {
      float a = (now % 60000) / 60000.0f * 360.0f;
      float r = cfg::RING_R * 0.62f;
      santaX_ = cfg::RADAR_C + sinf(a * D2R) * r;
      santaY_ = cfg::RADAR_C - cosf(a * D2R) * r;
      santaHdg_ = a + 90;
    }

    if (g_set.runways && (rwyRange_ != range_ || rwyUnits_ != units_ || rwyLat_ != lat0_ || rwyLon_ != lon0_)) {
      buildRunways();
    }
  }

  void buildRunways() {
    using namespace data::large_airports;
    rwyRange_ = range_;
    rwyUnits_ = units_;
    rwyLat_ = lat0_;
    rwyLon_ = lon0_;
    nRwy_ = nAp_ = 0;
    float limNm = (cfg::EDGE_LIM + 4) / pxPerNm_;
    for (size_t i = 0; i < kRunwayCount && nRwy_ < (int)(sizeof(rwys_) / sizeof(rwys_[0])); ++i) {
      const Runway& r = kRunways[i];
      Rwy w;
      nmOffset(r.le_lat_e7 / 1e7f, r.le_lon_e7 / 1e7f, w.x0, w.y0);
      nmOffset(r.he_lat_e7 / 1e7f, r.he_lon_e7 / 1e7f, w.x1, w.y1);
      bool in0 = fabsf(w.x0) < limNm && fabsf(w.y0) < limNm;
      bool in1 = fabsf(w.x1) < limNm && fabsf(w.y1) < limNm;
      if (!in0 && !in1) continue;
      rwys_[nRwy_++] = w;
      const Airport& ap = kAirports[r.airport_idx];
      bool have = false;
      for (int k = 0; k < nAp_; ++k) have |= strncmp(aps_[k].id, ap.ident, 4) == 0;
      if (!have && nAp_ < (int)(sizeof(aps_) / sizeof(aps_[0]))) {
        ApLabel& l = aps_[nAp_++];
        memcpy(l.id, ap.ident, 4);
        l.id[4] = 0;
        nmOffset(ap.lat_e7 / 1e7f, ap.lon_e7 / 1e7f, l.x, l.y);
      }
    }
  }

  // ---------------------------------------------------------------- radar
  void chip(Canvas& c, int x, int y, const char* txt, bool right, uint16_t dot = 0, bool planeIcon = false) {
    int tw = c.textWidth(txt, F_MONO8);
    int w = tw + 10 + (dot ? 8 : 0) + (planeIcon ? 10 : 0);
    int x0 = right ? x - w : x;
    c.roundRect(x0, y, w, 14, 3, ui_c::HUD_BG);
    c.roundFrame(x0, y, w, 14, 3, ui_c::BORDER);
    int tx = x0 + 5;
    if (dot) {
      c.smoothCircle(tx + 2, y + 7, 2, dot);
      tx += 8;
    }
    if (planeIcon) {
      icons::plane(c, tx + 3, y + 7, 0, 9, ui_c::AMBER);
      tx += 10;
    }
    c.text(txt, tx, y + 7, F_MONO8, ui_c::HUD_TEXT, lgfx::middle_left);
  }

  void drawLabel(Canvas& c, const Aircraft& a, const Pt& p, uint16_t col1, uint16_t col2) {
    char l1[12] = "", l2[28] = "";
    uint8_t lb = g_set.labels;
    if (lb & LBL_CALLSIGN) {
      snprintf(l1, sizeof(l1), "%s", a.flight[0] ? a.flight : a.reg[0] ? a.reg : "");
      if (!l1[0]) snprintf(l1, sizeof(l1), "%06X", (unsigned)a.hex);
    }
    if (isEmergency(a.squawk)) snprintf(l1 + strlen(l1), sizeof(l1) - strlen(l1), " %u", a.squawk);
    if ((lb & LBL_TYPE) && a.type[0]) strncat(l2, a.type, sizeof(l2) - strlen(l2) - 1);
    if (lb & LBL_ALT) {
      char t[12];
      if (a.ground) strcpy(t, "GND");
      else if (a.alt == ALT_UNKNOWN) t[0] = 0;
      else if (units_ == U_METRIC) snprintf(t, sizeof(t), "%.1fkm", a.alt * 0.0003048f);
      else snprintf(t, sizeof(t), "%.1fk", a.alt / 1000.0f);
      if (t[0]) {
        if (l2[0]) strncat(l2, " ", sizeof(l2) - strlen(l2) - 1);
        strncat(l2, t, sizeof(l2) - strlen(l2) - 1);
      }
    }
    if ((lb & LBL_SPEED) && a.gs > 0) {
      char t[12];
      if (units_ == U_METRIC) snprintf(t, sizeof(t), "%dkmh", (int)(a.gs * 1.852f));
      else if (units_ == U_IMPERIAL) snprintf(t, sizeof(t), "%dmph", (int)(a.gs * 1.15078f));
      else snprintf(t, sizeof(t), "%dkt", (int)a.gs);
      if (l2[0]) strncat(l2, " ", sizeof(l2) - strlen(l2) - 1);
      strncat(l2, t, sizeof(l2) - strlen(l2) - 1);
    }
    if (!l1[0] && !l2[0]) return;
    bool right = p.x > cfg::RADAR_C;  // labels point toward the center
    int lx = right ? (int)p.x - 10 : (int)p.x + 10;
    int lines = (l1[0] ? 1 : 0) + (l2[0] ? 1 : 0);
    int ly = (int)p.y - (lines == 2 ? 9 : 4);
    ly = constrain(ly, 16, 238 - lines * 9);
    auto datum = right ? lgfx::top_right : lgfx::top_left;
    if (l1[0]) {
      c.text(l1, lx, ly, F_MONO8, col1, datum);
      ly += 9;
    }
    if (l2[0]) c.text(l2, lx, ly, F_MONO8, col2, datum);
  }

  void drawRadar(Canvas& c) {
    const RadarTheme& T = theme();
    const int C = cfg::RADAR_C;
    c.fillRect(0, c.oy, 240, cfg::BAND_H, T.bg);
    c.fillRect(C - 1, 0, 2, 240, T.cross);
    c.fillRect(0, C - 1, 240, 2, T.cross);
    c.ring(C, C, cfg::RING_R, 1.6f, T.ring);
    c.ring(C, C, cfg::RING_R * 2 / 3, 1.3f, T.ringDim);
    c.ring(C, C, cfg::RING_R / 3, 1.3f, T.ringDim);

    // ring distance labels (south-east diagonal)
    char buf[24];
    for (int k = 1; k <= 3; ++k) {
      float r = cfg::RING_R * k / 3.0f;
      float v = range_ * k / 3.0f;
      if (k == 3) snprintf(buf, sizeof(buf), "%d %s", range_, distUnit(units_));
      else if (v >= 10) snprintf(buf, sizeof(buf), "%d", (int)lroundf(v));
      else snprintf(buf, sizeof(buf), "%.1f", v);
      c.text(buf, C + r * 0.707f + 3, C + r * 0.707f - 3, F_MONO8, T.ringLabel, lgfx::bottom_left);
    }

    if (g_set.runways && g_set.locValid) {
      for (int i = 0; i < nRwy_; ++i) {
        const Rwy& w = rwys_[i];
        c.wideLine(C + w.x0 * pxPerNm_, C - w.y0 * pxPerNm_, C + w.x1 * pxPerNm_, C - w.y1 * pxPerNm_, 1.0f, T.runway);
      }
      for (int i = 0; i < nAp_; ++i) {
        float x = C + aps_[i].x * pxPerNm_, y = C - aps_[i].y * pxPerNm_;
        if (fabsf(x - C) < 105 && fabsf(y - C) < 105) c.text(aps_[i].id, x + 6, y + 4, F_MONO8, T.runwayLabel);
      }
    }

    // compass letters
    const struct { const char* t; int x, y; } cl[] = {{"N", C, 9}, {"E", 232, C}, {"S", C, 231}, {"W", 8, C}};
    for (auto& k : cl) {
      if (!c.visible(k.y - 6, 12)) continue;
      c.fillRect(k.x - 5, k.y - 6, 11, 12, T.bg);
      c.text(k.t, k.x, k.y, F_UI11, T.compass, lgfx::middle_center);
    }

    if (g_set.theme == T_HOLIDAY) {
      for (auto& f : snow_) c.fillRect((int)f.x, (int)f.y, 2, 2, rgb(235, 245, 255));
    }

    // trails
    if (g_set.trails) {
      for (int i = 0; i < trailCount_ && i < acCount_; ++i) {
        if (!pts_[i].show || !pts_[i].inside) continue;
        const Trail& t = trails_[i];
        if (t.hex != ac_[i].hex) continue;
        float px = 0, py = 0;
        for (int k = 0; k < t.n; ++k) {
          float x = C + t.x[k] / 100.0f * pxPerNm_, y = C - t.y[k] / 100.0f * pxPerNm_;
          if (k > 0) c.smoothLine(px, py, x, y, T.trail);
          px = x;
          py = y;
        }
        c.smoothLine(px, py, pts_[i].x, pts_[i].y, T.trail);
      }
    }

    // home
    c.ring(C, C, 4, 1.4f, T.home);
    c.smoothCircle(C, C, 1, T.home);

    // aircraft (selected one last so it sits on top)
    int selIdx = -1;
    for (int pass = 0; pass < 2; ++pass) {
      for (int i = 0; i < acCount_; ++i) {
        const Pt& p = pts_[i];
        if (!p.show) continue;
        bool sel = ac_[i].hex == selHex_;
        if (sel) selIdx = i;
        if (sel != (pass == 1)) continue;
        if (!c.visible((int)p.y - 30, 60)) continue;
        bool emerg = isEmergency(ac_[i].squawk);
        bool mil = ac_[i].military && g_set.milHighlight;
        uint16_t col = sel ? T.planeSel : emerg ? ui_c::RED : mil ? militaryColor() : T.plane;
        if (!p.inside) {
          c.smoothCircle((int)p.x, (int)p.y, 3, col);
          continue;
        }
        if (!ac_[i].ground && ac_[i].gs > 30) {
          float len = ac_[i].gs / 16.0f;
          float a = ac_[i].track * D2R;
          c.smoothLine(p.x, p.y, p.x + sinf(a) * len, p.y - cosf(a) * len, T.vector);
        }
        if (sel) c.ring((int)p.x, (int)p.y, 11, 1.5f, T.planeSel);
        icons::plane(c, p.x, p.y, ac_[i].track, 13, col);
        drawLabel(c, ac_[i], p, sel ? T.planeSel : emerg ? ui_c::RED : mil ? militaryColor() : T.label, T.labelSub);
      }
    }
    (void)selIdx;

    if (santaOn_ && c.visible((int)santaY_ - 20, 40)) {
      if (selHex_ == SANTA_HEX) c.ring((int)santaX_, (int)santaY_, 13, 1.5f, T.planeSel);
      icons::sleigh(c, santaX_, santaY_, santaHdg_, rgb(220, 40, 50), rgb(255, 215, 90));
      c.text("SANTA", santaX_, santaY_ + 7, F_MONO8, rgb(255, 215, 90), lgfx::top_center);
    }

    if (!g_set.locValid) {
      if (c.visible(95, 50)) {
        c.roundRect(40, 95, 160, 50, 8, ui_c::HUD_BG);
        c.roundFrame(40, 95, 160, 50, 8, ui_c::BORDER);
        c.text("Where should I look?", C, 111, F_UI11, ui_c::TEXT, lgfx::middle_center);
        c.text("Tap here to set your location", C, 129, F_UI9, ui_c::TEXT_DIM, lgfx::middle_center);
      }
    }

    // HUD corners
    if (c.visible(0, 22)) {
      snprintf(buf, sizeof(buf), "%d %s", range_, distUnit(units_));
      chip(c, 4, 4, buf, false);
      snprintf(buf, sizeof(buf), "%d", count_);
      chip(c, 236, 4, buf, true, 0, true);
    }
    if (c.visible(218, 22)) {
      struct tm t;
      if (net::localTime(t)) {
        int h = t.tm_hour;
        if (g_set.clock24) snprintf(buf, sizeof(buf), "%02d:%02d", h, t.tm_min);
        else snprintf(buf, sizeof(buf), "%d:%02d %s", h % 12 == 0 ? 12 : h % 12, t.tm_min, h < 12 ? "AM" : "PM");
        chip(c, 4, 222, buf, false);
      }
      uint16_t dot;
      const char* st;
      uint8_t w = g_net.wifi;
      if (w == W_NO_CREDS) { st = "No Wi-Fi"; dot = ui_c::RED; }
      else if (w == W_FAILED) { st = g_net.failReason == FAIL_PASSWORD ? "Wi-Fi password?" : "Wi-Fi?"; dot = ui_c::RED; }
      else if (w == W_CONNECTING) { st = "Connecting"; dot = ui_c::AMBER; }
      else if (!g_set.locValid) { st = "No location"; dot = ui_c::AMBER; }
      else if (g_adsb.okMs && millis() - g_adsb.okMs < 30000) { st = sourceName(g_set.source); dot = ui_c::GREEN; }
      else { st = "Waiting"; dot = ui_c::AMBER; }
      chip(c, 236, 222, st, true, dot);
    }
  }

  // ---------------------------------------------------------------- strip
  void drawStrip(Canvas& c) {
    c.fillRect(0, cfg::STRIP_Y, 240, cfg::STRIP_H, ui_c::STRIP_BG);
    c.hline(0, cfg::STRIP_Y, 240, ui_c::BORDER);
    if (selHex_) drawDetails(c);
    else drawWeather(c);
  }

  void drawWeather(Canvas& c) {
    Weather w;
    {
      Lock l;
      w = g_weather;
    }
    const int y0 = cfg::STRIP_Y;
    if (!g_set.locValid) {
      c.text("Set your location", 10, y0 + 30, F_UI11, ui_c::TEXT, lgfx::middle_left);
      c.text("Tap the gear, then Location", 10, y0 + 47, F_UI9, ui_c::TEXT_DIM, lgfx::middle_left);
    } else if (!w.valid) {
      if (g_net.wifi != W_CONNECTED) {
        c.text("Weather needs Wi-Fi", 10, y0 + 40, F_UI11, ui_c::TEXT_DIM, lgfx::middle_left);
      } else if (w.fails > 0 && !w.loading) {
        c.text("Weather service", 10, y0 + 22, F_UI11, ui_c::TEXT, lgfx::middle_left);
        c.text("isn't answering", 10, y0 + 37, F_UI11, ui_c::TEXT, lgfx::middle_left);
        char m[40];
        snprintf(m, sizeof(m), "Retrying every 15 s (try %u)", (unsigned)w.fails + 1);
        c.text(m, 10, y0 + 58, F_UI8, ui_c::TEXT_DIM, lgfx::middle_left);
      } else {
        c.text("Loading weather...", 10, y0 + 40, F_UI11, ui_c::TEXT_DIM, lgfx::middle_left);
      }
    } else {
      icons::weather(c, 22, y0 + 19, w.code, w.isDay, ui_c::STRIP_BG);
      char t[16];
      snprintf(t, sizeof(t), "%d\xC2\xB0", (int)lroundf(w.temp));
      int tw = c.text(t, 40, y0 + 20, F_TEMP, ui_c::TEXT, lgfx::middle_left);
      int x2 = 40 + tw + 5, avail = 156 - x2;
      char cond[24];
      strncpy(cond, wmoText(w.code), sizeof(cond) - 1);
      cond[sizeof(cond) - 1] = 0;
      FontId cf = c.textWidth(cond, F_UI11) <= avail ? F_UI11 : F_UI9;
      fitText(c, cond, sizeof(cond), cf, avail);
      c.text(cond, x2, y0 + 13, cf, ui_c::TEXT, lgfx::middle_left);
      snprintf(t, sizeof(t), "H %d\xC2\xB0  L %d\xC2\xB0", (int)lroundf(w.tmax), (int)lroundf(w.tmin));
      c.text(t, x2, y0 + 27, F_UI9, ui_c::TEXT_DIM, lgfx::middle_left);

      // city on its own centered line
      char city[48];
      if (g_set.place[0]) strncpy(city, g_set.place, sizeof(city) - 1);
      else snprintf(city, sizeof(city), "%.3f, %.3f", g_set.lat, g_set.lon);
      city[sizeof(city) - 1] = 0;
      FontId pf = c.textWidth(city, F_UI13) <= 148 ? F_UI13 : F_UI11;
      fitText(c, city, sizeof(city), pf, 148);
      c.text(city, 80, y0 + 47, pf, ui_c::TEXT, lgfx::middle_center);

      const char* su = w.units == U_METRIC ? "km/h" : w.units == U_AVIATION ? "kt" : "mph";
      char line[64];
      snprintf(line, sizeof(line), "Wind %s %d %s \xC2\xB7 Hum %d%%", compass8(w.windDir), (int)lroundf(w.wind), su,
               w.humidity);
      if (c.textWidth(line, F_UI9) > 148)
        snprintf(line, sizeof(line), "%s %d %s \xC2\xB7 %d%%", compass8(w.windDir), (int)lroundf(w.wind), su, w.humidity);
      c.text(line, 80, y0 + 66, F_UI9, rgb(183, 199, 216), lgfx::middle_center);
    }

    // weather service status dot, top-right of the weather area
    if (g_set.locValid && g_net.wifi == W_CONNECTED) {
      uint16_t dot;
      if (w.fails > 0 && !w.loading) dot = ui_c::RED;
      else if (w.loading || !w.valid || millis() - w.fetchedMs > 20 * 60 * 1000UL) dot = ui_c::AMBER;
      else dot = ui_c::GREEN;
      c.smoothCircle(152, y0 + 7, 3, dot);
    }

    // zoom + settings buttons
    bool canIn = rangeIdx_ > 0, canOut = rangeIdx_ < RANGE_COUNT - 1;
    c.fillRect(160, y0 + 1, 40, 39, pressedIn(160, y0, 40, 40) ? ui_c::BTN_PRESSED : ui_c::BTN_BG);
    c.fillRect(160, y0 + 41, 40, 39, pressedIn(160, y0 + 40, 40, 40) ? ui_c::BTN_PRESSED : ui_c::BTN_BG);
    c.fillRect(201, y0 + 1, 39, 79, pressedIn(200, y0, 40, 80) ? ui_c::BTN_PRESSED : ui_c::BTN_BG);
    c.vline(159, y0, 80, ui_c::BORDER);
    c.vline(200, y0, 80, ui_c::BORDER);
    c.hline(160, y0 + 40, 40, ui_c::BORDER);
    icons::plus(c, 180, y0 + 20, 16, canIn ? ui_c::HUD_TEXT : ui_c::TEXT_FAINT);
    icons::minus(c, 180, y0 + 60, 16, canOut ? ui_c::HUD_TEXT : ui_c::TEXT_FAINT);
    icons::gear(c, 220, y0 + 40, 10, ui_c::HUD_TEXT, ui_c::BTN_BG);
  }

  void drawDetails(Canvas& c) {
    const int y0 = cfg::STRIP_Y;
    const int px = 7, py = y0 + 9;
    const int tx = 104, tw = 132;
    uint32_t now = millis();
    char reg[16] = "", typeName[40] = "", airline[40] = "", route[40] = "", stats[40] = "", callsign[12] = "";
    char credit[32] = "";
    bool loading = false, photoLoading = false, photoNone = false, photoFailed = false, military = false,
         similar = false;

    c.roundRect(px - 1, py - 1, cfg::PHOTO_W + 2, cfg::PHOTO_H + 2, 3, ui_c::PHOTO_BORDER);
    if (selHex_ == SANTA_HEX) {
      icons::sleighArt(c, px, py, cfg::PHOTO_W, cfg::PHOTO_H);
      strcpy(reg, "HO-HO-HO");
      strcpy(callsign, "SANTA1");
      strcpy(typeName, "Sleigh, 9 reindeer");
      strcpy(airline, "North Pole Express");
      strcpy(route, "NORTH POLE \xE2\x86\x92 EVERYWHERE");
      strcpy(stats, "Speed: very fast");
    } else {
      const Aircraft* a = nullptr;
      int idx = -1;
      for (int i = 0; i < acCount_; ++i) {
        if (ac_[i].hex == selHex_) {
          a = &ac_[i];
          idx = i;
          break;
        }
      }
      {
        Lock l;
        if (g_details.hex == selHex_) {
          strncpy(reg, g_details.reg, sizeof(reg) - 1);
          strncpy(typeName, g_details.typeName, sizeof(typeName) - 1);
          strncpy(airline, g_details.airline, sizeof(airline) - 1);
          strncpy(route, g_details.route, sizeof(route) - 1);
          strncpy(credit, g_details.photographer, sizeof(credit) - 1);
          loading = g_details.state != D_DONE;
          photoLoading = g_details.photoLoading;
          photoNone = g_details.photoNone;
          photoFailed = g_details.photoFailed;
          similar = g_details.photoSimilar;
          if (g_details.photoReady && g_details.photoSerial != photoSerial_) {
            photoSerial_ = g_details.photoSerial;
            photo_.fillScreen(ui_c::PHOTO_BG);
            photoOk_ = photo_.drawJpg(g_details.photo, g_details.photoLen, 0, 0, cfg::PHOTO_W, cfg::PHOTO_H, 0, 0, 0.0f,
                                      0.0f, lgfx::datum_t::middle_center);
            if (!photoOk_) {
              photoNone = true;
              Serial.printf("photo: decode failed (%u bytes)\n", (unsigned)g_details.photoLen);
            }
          }
        }
      }
      if (a) {
        military = a->military;
        if (!reg[0]) strncpy(reg, a->reg, sizeof(reg) - 1);
        strncpy(callsign, a->flight, sizeof(callsign) - 1);
        if (!typeName[0]) strncpy(typeName, a->desc[0] ? a->desc : a->type, sizeof(typeName) - 1);
        if (a->ground) strcpy(stats, "On the ground");
        else {
          char alt[16] = "", spd[16] = "";
          if (a->alt != ALT_UNKNOWN) {
            if (units_ == U_METRIC) snprintf(alt, sizeof(alt), "%d m", (int)(a->alt * 0.3048f));
            else {
              int v = a->alt;
              snprintf(alt, sizeof(alt), v >= 1000 ? "%d,%03d ft" : "%d ft", v >= 1000 ? v / 1000 : v, v % 1000);
            }
          }
          if (units_ == U_METRIC) snprintf(spd, sizeof(spd), "%d km/h", (int)(a->gs * 1.852f));
          else if (units_ == U_IMPERIAL) snprintf(spd, sizeof(spd), "%d mph", (int)(a->gs * 1.15078f));
          else snprintf(spd, sizeof(spd), "%d kt", (int)a->gs);
          snprintf(stats, sizeof(stats), "%s%s%s", alt, alt[0] ? " \xC2\xB7 " : "", spd);
        }
        if (!route[0] && idx >= 0 && pts_[idx].show) {
          float d = sqrtf(pts_[idx].nmX * pts_[idx].nmX + pts_[idx].nmY * pts_[idx].nmY) * unitsPerNm(units_);
          float brg = atan2f(pts_[idx].nmX, pts_[idx].nmY) / D2R;
          snprintf(route, sizeof(route), "%.1f %s %s of you", d, distUnit(units_), compass8(brg));
        }
      }
      if (!reg[0]) snprintf(reg, sizeof(reg), "%06X", (unsigned)selHex_);
      if (!airline[0]) strcpy(airline, loading ? "Looking up..." : military ? "Military" : "");

      // photo
      if (photoOk_ && photoSerial_ && !photoNone) {
        photo_.pushSprite(c.s, px, py - c.oy);
        if (credit[0] || similar) {
          char cr[48];
          if (similar && credit[0]) snprintf(cr, sizeof(cr), "Same type \xC2\xB7 \xC2\xA9 %s", credit);
          else if (similar) strcpy(cr, "Same type, not this plane");
          else snprintf(cr, sizeof(cr), "\xC2\xA9 %s", credit);
          c.fillRect(px, py + cfg::PHOTO_H - 9, cfg::PHOTO_W, 9, ui_c::HUD_BG);
          fitText(c, cr, sizeof(cr), F_UI8, cfg::PHOTO_W - 4);
          c.text(cr, px + 2, py + cfg::PHOTO_H - 5, F_UI8, ui_c::TEXT_DIM, lgfx::middle_left);
        }
      } else {
        c.fillRect(px, py, cfg::PHOTO_W, cfg::PHOTO_H, ui_c::PHOTO_BG);
        icons::planeSide(c, px + 20, py + 12, 48, rgb(74, 106, 140));
        const char* msg = (photoLoading || loading) ? "Loading photo..."
                          : (photoFailed || photoSerial_) ? "Photo failed"
                                                          : "No photo";
        c.text(msg, px + cfg::PHOTO_W / 2, py + 48, F_UI8, ui_c::TEXT_DIM, lgfx::middle_center);
      }
    }

    // text column
    int rw = c.text(reg, tx, y0 + 14, F_MONO15, rgb(255, 255, 255), lgfx::middle_left);
    int chipX = tx + rw + 5;
    if (callsign[0] && strcmp(callsign, reg) != 0) {
      int cw = c.textWidth(callsign, F_MONO8) + 8;
      if (chipX + cw <= 222) {
        c.roundFrame(chipX, y0 + 8, cw, 13, 2, rgb(43, 106, 132));
        c.text(callsign, chipX + cw / 2, y0 + 14, F_MONO8, ui_c::CYAN, lgfx::middle_center);
        chipX += cw + 4;
      }
    }
    if (military) {  // tag on the photo's top-left corner
      int w = c.textWidth("MILITARY", F_MONO8) + 8;
      c.roundRect(px + 2, py + 2, w, 12, 2, militaryColor());
      c.text("MILITARY", px + 2 + w / 2, py + 8, F_MONO8, ui_c::STRIP_BG, lgfx::middle_center);
    }
    fitText(c, typeName, sizeof(typeName), F_UI11, tw);
    c.text(typeName, tx, y0 + 31, F_UI11, ui_c::TEXT, lgfx::middle_left);
    fitText(c, airline, sizeof(airline), F_UI9, tw);
    c.text(airline, tx, y0 + 44, F_UI9, ui_c::TEXT_DIM, lgfx::middle_left);
    fitText(c, route, sizeof(route), F_MONO8, tw);
    c.text(route, tx, y0 + 56, F_MONO8, rgb(220, 230, 240), lgfx::middle_left);
    fitText(c, stats, sizeof(stats), F_MONO8, tw);
    c.text(stats, tx, y0 + 67, F_MONO8, ui_c::AMBER, lgfx::middle_left);

    // countdown
    uint32_t total = POPUP_SECONDS[g_set.popupIdx] * 1000;
    int32_t left = (int32_t)(selUntil_ - now);
    if (left < 0) left = 0;
    char secs[8];
    snprintf(secs, sizeof(secs), "%lus", (unsigned long)((left + 999) / 1000));
    c.text(secs, 236, y0 + 9, F_MONO8, ui_c::TEXT_DIM, lgfx::top_right);
    c.fillRect(0, y0 + 78, (int)(240.0f * left / total), 2, ui_c::CYAN);
  }

  // ---------------------------------------------------------------- state
  bool inited_ = false;
  int rangeIdx_ = 2;
  int range_ = 10;
  uint8_t units_ = 0;
  float pxPerNm_ = 1;
  float lat0_ = 0, lon0_ = 0, cosLat_ = 1;
  int count_ = 0;

  Aircraft ac_[MAXA];
  int acCount_ = 0;
  uint32_t acVersion_ = 0;
  Pt pts_[MAXA];
  Trail trails_[MAXA];
  int trailCount_ = 0;

  uint32_t selHex_ = 0, selUntil_ = 0, selStart_ = 0;
  lgfx::LGFX_Sprite photo_{&display::lcd};
  uint32_t photoSerial_ = 0;
  bool photoOk_ = false;

  Rwy rwys_[48];
  ApLabel aps_[12];
  int nRwy_ = 0, nAp_ = 0;
  int rwyRange_ = -1;
  uint8_t rwyUnits_ = 255;
  float rwyLat_ = 0, rwyLon_ = 0;

  Flake snow_[36];
  Alerted alerted_[24] = {};
  int alertPos_ = 0;

  bool santaOn_ = false;
  float santaX_ = 0, santaY_ = 0, santaHdg_ = 0;
  uint32_t lastSecond_ = 0;
};

RadarScreen s_radar;

}  // namespace

Screen* radarScreen() { return &s_radar; }

}  // namespace ui
