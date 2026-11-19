// Host stand-ins for board, network, portal and audio
#include "board.h"
#include "net.h"
#include "portal.h"
#include "audio.h"
#include <WiFi.h>
#include <ctime>

uint32_t g_simMillis = 100000;
SerialStub Serial;
EspStub ESP;
WiFiStub WiFi;

SemaphoreHandle_t g_lock;
AdsbData g_adsb;
Weather g_weather;
Details g_details;
NetStatus g_net;
void stateInit() {}

int g_simMonth = 10, g_simDay = 14, g_simHour = 19, g_simMin = 22;
uint32_t g_lastRequestedHex = 0;

namespace board {
bool begin() { return true; }
void resetLcd() {}
void setBacklight(uint8_t) {}
void setAmp(bool) {}
bool readBattery(float& v) { v = 4.07f; return true; }
bool readTouch(int16_t&, int16_t&) { return false; }
}

namespace net {
void begin() {}
void reconnect() {}
void pauseRetries(bool) {}
void requestGeocode(const char*) {}
void refreshWeather() {}
void refreshAdsb() {}
void setFetchRadiusNm(float) {}
void releasePhoto() {}
void requestDetails(const Aircraft& ac) {
  g_details = Details{};
  g_details.hex = ac.hex;
  g_details.state = D_LOADING;
  strncpy(g_details.reg, ac.reg, sizeof(g_details.reg) - 1);
  g_lastRequestedHex = ac.hex;
}
bool localTime(struct tm& t) {
  memset(&t, 0, sizeof(t));
  t.tm_year = 126; t.tm_mon = g_simMonth; t.tm_mday = g_simDay; t.tm_hour = g_simHour; t.tm_min = g_simMin;
  return true;
}
}

namespace portal {
void start() {}
void stop() {}
bool active() { return true; }
void loop() {}
const char* apName() { return "FlightRadar-3F2A"; }
bool take(Submission&) { return false; }
}

namespace audio {
void begin() {}
void play(Chime) {}
}

struct FakeNet { const char* ssid; int rssi; bool open; };
static const FakeNet NETS[] = {{"HomeNet", -48, false}, {"Smith_Family", -61, false}, {"Office-Guest", -70, false},
                               {"xfinitywifi", -72, true}, {"ATT-2G-8841", -81, false}, {"NETGEAR42", -84, false}};
int WiFiStub::scanComplete() { return 6; }
String WiFiStub::SSID(int i) { return NETS[i].ssid; }
int WiFiStub::RSSI(int i) { return NETS[i].rssi; }
int WiFiStub::encryptionType(int i) { return NETS[i].open ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK; }
