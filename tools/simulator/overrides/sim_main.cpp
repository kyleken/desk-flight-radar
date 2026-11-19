// Renders every firmware screen to an image using the real UI code.
// Built and run by build.sh; SIM_OUT and SIM_PHOTO are passed in as -D defines.
#include <cmath>
#include <cstdio>
#include <vector>
#include "display.h"
#include "settings.h"
#include "state.h"
#include "ui.h"

extern uint32_t g_simMillis;
extern int g_simMonth, g_simDay, g_simHour, g_simMin;
extern uint32_t g_lastRequestedHex;
extern uint32_t g_santaPreviewUntil;

static const double LAT0 = 41.8858, LON0 = -87.6181;

struct Mock {
  const char* flight; const char* reg; const char* type; const char* desc;
  float distMi, brg, hdg; int alt; float gs; uint16_t squawk;
};
static const Mock MOCK[] = {
  {"UAL1423", "N37502", "B38M", "BOEING 737 MAX 8", 6.0f, 40, 250, 12400, 288, 1200},
  {"AAL2271", "N137AA", "A321", "", 3.5f, 200, 95, 4200, 212, 0},
  {"SWA918", "N7841A", "B737", "", 8.4f, 300, 125, 23000, 410, 0},
  {"N172SP", "N172SP", "C172", "", 2.1f, 115, 330, 2500, 104, 0},
  {"ENY3420", "N401YX", "E75L", "", 9.2f, 18, 205, 8900, 256, 0},
  {"FDX1211", "N119FE", "B763", "", 12.3f, 150, 340, 6100, 238, 0},
  {"DAL562", "N391DN", "A321", "", 30.0f, 62, 330, 35000, 470, 0},
  {"RCH431", "05-5140", "C17", "BOEING C-17A GLOBEMASTER 3", 7.5f, 250, 70, 15200, 330, 0},
};
constexpr int NMOCK = sizeof(MOCK) / sizeof(MOCK[0]);

void loadAircraft(float secondsAgo, int squawkIdx = -1) {
  for (int i = 0; i < NMOCK; ++i) {
    const Mock& m = MOCK[i];
    Aircraft& a = g_adsb.list[i];
    a = Aircraft{};
    a.hex = (i == NMOCK - 1) ? 0xAE1234 : 0xA00000 + i * 0x1111;
    a.military = (i == NMOCK - 1);
    strcpy(a.flight, m.flight);
    strcpy(a.reg, m.reg);
    strcpy(a.type, m.type);
    strcpy(a.desc, m.desc);
    float nm = m.distMi / 1.15078f;
    // move back along track for older samples
    float back = m.gs * secondsAgo / 3600.0f;
    float nx = nm * sinf(m.brg * DEG_TO_RAD) - back * sinf(m.hdg * DEG_TO_RAD);
    float ny = nm * cosf(m.brg * DEG_TO_RAD) - back * cosf(m.hdg * DEG_TO_RAD);
    a.lat = LAT0 + ny / 60.0;
    a.lon = LON0 + nx / (60.0 * cos(LAT0 * DEG_TO_RAD));
    a.track = m.hdg;
    a.gs = m.gs;
    a.alt = m.alt;
    a.squawk = (i == squawkIdx) ? 7700 : m.squawk;
    a.posMs = g_simMillis;
  }
  g_adsb.count = NMOCK;
  g_adsb.version++;
  g_adsb.okMs = g_simMillis;
}

void screenPos(int i, int rangeMi, float& x, float& y) {
  float px = 108.0f / rangeMi;  // px per mile
  x = 120 + MOCK[i].distMi * sinf(MOCK[i].brg * DEG_TO_RAD) * px;
  y = 120 - MOCK[i].distMi * cosf(MOCK[i].brg * DEG_TO_RAD) * px;
}

void shot(const char* name) {
  ui::invalidate();
  ui::loop();
  char path[256];
  snprintf(path, sizeof(path), "%s/%s.ppm", SIM_OUT, name);
  FILE* f = fopen(path, "wb");
  fprintf(f, "P6\n240 320\n255\n");
  for (int y = 0; y < 320; ++y)
    for (int x = 0; x < 240; ++x) {
      auto c = display::lcd.readPixelRGB(x, y);
      unsigned char px[3] = {c.r, c.g, c.b};
      fwrite(px, 1, 3, f);
    }
  fclose(f);
  printf("wrote %s\n", name);
}

std::vector<uint8_t> readFile(const char* p) {
  std::vector<uint8_t> v;
  FILE* f = fopen(p, "rb");
  if (!f) return v;
  int c;
  while ((c = fgetc(f)) != EOF) v.push_back((uint8_t)c);
  fclose(f);
  return v;
}

int main() {
  settingsLoad();
  strcpy(g_set.ssid, "HomeNet");
  strcpy(g_set.pass, "secret");
  strcpy(g_set.zip, "60601");
  strcpy(g_set.place, "Chicago, IL");
  g_set.lat = LAT0;
  g_set.lon = LON0;
  g_set.locValid = true;

  g_net.wifi = W_CONNECTED;
  strcpy(g_net.ip, "192.168.1.42");
  g_net.rssi = -52;
  g_net.timeSynced = true;
  g_net.utcOffsetKnown = true;

  g_weather.valid = true;
  g_weather.temp = 38; g_weather.tmax = 44; g_weather.tmin = 31;
  g_weather.wind = 14; g_weather.windDir = 315; g_weather.humidity = 62;
  g_weather.code = 2; g_weather.isDay = false;

  display::begin(false);
  loadAircraft(0);
  ui::begin();
  shot("01_radar_home");

  // tap UAL1423 -> details
  float x, y;
  screenPos(0, 10, x, y);
  ui::radarScreen()->onTap((int)x, (int)y);
  printf("tapped at %.0f,%.0f -> hex %06X\n", x, y, g_lastRequestedHex);
  static std::vector<uint8_t> jpg = readFile(SIM_PHOTO);
  g_details.state = D_DONE;
  strcpy(g_details.typeName, "Boeing 737 MAX 8");
  strcpy(g_details.airline, "United Airlines");
  strcpy(g_details.route, "DEN \xE2\x86\x92 ORD");
  strcpy(g_details.photographer, "J. Spotter");
  g_details.photo = jpg.data();
  g_details.photoLen = jpg.size();
  g_details.photoReady = !jpg.empty();
  g_details.photoSerial = 1;
  g_details.photoSimilar = true;
  g_simMillis += 1600;
  shot("02_radar_details");

  // details still loading (another plane)
  g_simMillis += 5000;
  ui::invalidate();
  ui::loop();
  screenPos(3, 10, x, y);
  ui::radarScreen()->onTap((int)x, (int)y);
  g_simMillis += 300;
  shot("03_radar_details_loading");
  g_details.state = D_DONE;
  g_details.photoLoading = false;
  g_details.photoNone = true;
  g_details.photoFailed = true;
  g_simMillis += 300;
  shot("03b_radar_details_photo_failed");
  g_details.photoFailed = false;
  g_simMillis += 6000;
  ui::loop();

  // zoom out to 25 mi (runways)
  ui::radarScreen()->onTap(180, 300);
  g_simMillis += 300;
  shot("04_radar_25mi");
  ui::radarScreen()->onTap(180, 260);  // back to 10

  // tap the military C-17
  loadAircraft(0);
  ui::invalidate();
  ui::loop();
  screenPos(NMOCK - 1, 10, x, y);
  ui::radarScreen()->onTap((int)x, (int)y);
  printf("mil tap %.0f,%.0f -> %06X\n", x, y, g_lastRequestedHex);
  g_details.state = D_DONE;
  strcpy(g_details.typeName, "Boeing C-17A Globemaster III");
  strcpy(g_details.airline, "United States Air Force");
  g_details.photoNone = true;
  g_simMillis += 400;
  shot("04b_radar_military");
  g_simMillis += 20000;
  ui::loop();

  ui::go(ui::SCR_SETTINGS);
  shot("05_settings");
  ui::go(ui::SCR_MILITARY);
  shot("05b_military");
  ui::openLocation(false);
  shot("06_location_zip");
  ui::locationScreen()->onTap(180, 57);
  shot("07_location_latlong");
  ui::go(ui::SCR_WIFI);
  ui::loop();
  shot("08_wifi");
  ui::openJoin("Smith_Family", false);
  ui::wifiJoinScreen()->onTap(49, 233);   // s
  ui::wifiJoinScreen()->onTap(165, 267);  // n
  shot("09_wifi_join");
  ui::go(ui::SCR_RADAR_PREFS);
  shot("10_radar_prefs");
  ui::go(ui::SCR_DISPLAY_PREFS);
  shot("11_display_prefs");
  ui::go(ui::SCR_NIGHT);
  shot("11b_night_dimming");
  ui::go(ui::SCR_ABOUT);
  shot("12_about");
  ui::go(ui::SCR_SETUP);
  shot("13_setup_qr");

  // Holiday theme + trails + Santa on Dec 24
  g_set.theme = T_HOLIDAY;
  g_set.trails = true;
  g_simMonth = 11;
  g_simDay = 24;
  ui::go(ui::SCR_RADAR);
  for (int k = 4; k >= 0; --k) {
    g_simMillis += 1100;
    loadAircraft(k * 5.0f);
    ui::loop();
  }
  for (int k = 0; k < 20; ++k) {  // let the snow fall a bit
    g_simMillis += 210;
    ui::loop();
  }
  shot("14_radar_holiday_santa");

  // Amber theme, metric, speed labels, an emergency squawk
  g_set.theme = T_AMBER;
  g_set.units = U_METRIC;
  g_set.labels = LBL_CALLSIGN | LBL_TYPE | LBL_ALT | LBL_SPEED;
  g_set.santa = false;
  g_simMillis += 1100;
  loadAircraft(0, 2);
  g_weather.units = U_METRIC;
  g_weather.temp = 3; g_weather.tmax = 7; g_weather.tmin = -1; g_weather.wind = 22; g_weather.code = 71;
  shot("15_radar_amber_metric");

  // First run: no location yet
  g_set.theme = T_CLASSIC;
  g_set.units = U_IMPERIAL;
  g_set.locValid = false;
  g_weather.valid = false;
  g_adsb.count = 0;
  g_adsb.version++;
  shot("16_radar_no_location");
  g_set.locValid = true;
  g_weather.valid = false; g_weather.fails = 3; g_weather.loading = false; g_weather.lastHttp = -1;
  shot("16b_weather_failing");
  return 0;
}
