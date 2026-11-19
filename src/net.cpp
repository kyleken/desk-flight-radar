// Network task. Runs beside the UI (same priority, time-sliced) so slow
// HTTPS requests never freeze the touch screen.
#include "net.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>
#include "settings.h"

SemaphoreHandle_t g_lock;
AdsbData g_adsb;
Weather g_weather;
Details g_details;
NetStatus g_net;

void stateInit() { g_lock = xSemaphoreCreateMutex(); }

namespace net {
namespace {

volatile bool s_reconnect = false;
volatile bool s_weatherNow = true;
volatile bool s_adsbNow = true;
volatile bool s_geoReq = false;
volatile bool s_detailsReq = false;
volatile float s_radiusNm = 15.0f;
char s_geoZip[6] = "";
Aircraft s_detailsAc;

AdsbData s_work;  // scratch for parsing (kept off the task stack)
uint32_t s_lastAdsb = 0, s_lastWeather = 0, s_lastRssi = 0;
bool s_ntpStarted = false;

WiFiClientSecure* s_tls = nullptr;
WiFiClient* s_plain = nullptr;

// Buffered reader so ArduinoJson doesn't pull a TLS stream one byte at a time.
struct BufReader {
  Client& c;
  uint8_t buf[1024];
  size_t n = 0, i = 0;
  uint32_t deadline;
  bool abortOnTap;
  BufReader(Client& cl, bool abortable) : c(cl), deadline(millis() + 15000), abortOnTap(abortable) {}
  bool refill() {
    i = n = 0;
    while ((int32_t)(millis() - deadline) < 0) {
      if (abortOnTap && s_detailsReq) return false;  // a tap beats a radar refresh
      int avail = c.available();
      if (avail > 0) {
        int r = c.read(buf, avail > (int)sizeof(buf) ? sizeof(buf) : avail);
        if (r > 0) { n = r; return true; }
      } else if (!c.connected()) {
        return false;
      }
      delay(2);
    }
    return false;
  }
  int read() {
    if (i >= n && !refill()) return -1;
    return buf[i++];
  }
  size_t readBytes(char* out, size_t len) {
    size_t got = 0;
    while (got < len) {
      int ch = read();
      if (ch < 0) break;
      out[got++] = (char)ch;
    }
    return got;
  }
};

bool beginHttp(HTTPClient& http, const char* url) {
  bool https = strncmp(url, "https", 5) == 0;
  WiFiClient* client = https ? (WiFiClient*)s_tls : s_plain;
  if (!http.begin(*client, url)) return false;
  http.useHTTP10(true);  // no chunked encoding, connection closes at the end
  http.setTimeout(10000);
  http.setConnectTimeout(8000);
  http.setUserAgent(cfg::USER_AGENT);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  return true;
}

bool getJson(const char* url, JsonDocument& doc, JsonDocument* filter, int* code = nullptr,
             bool abortOnTap = false) {
  HTTPClient http;
  if (!beginHttp(http, url)) return false;
  http.addHeader("Accept", "application/json");
  int c = http.GET();
  if (code) *code = c;
  if (c != 200) {
    http.end();
    return false;
  }
  BufReader reader(*http.getStreamPtr(), abortOnTap);
  DeserializationError err = filter
      ? deserializeJson(doc, reader, DeserializationOption::Filter(*filter))
      : deserializeJson(doc, reader);
  http.end();
  if (err) Serial.printf("json %s: %s\n", url, err.c_str());
  return !err;
}

// Download a small file into a malloc'd buffer (caller frees).
bool getBytes(const char* url, uint8_t** out, size_t* outLen, size_t maxLen, int* code = nullptr) {
  HTTPClient http;
  if (code) *code = 0;
  if (!beginHttp(http, url)) return false;
  int c = http.GET();
  if (code) *code = c;
  if (c != 200) {
    http.end();
    return false;
  }
  int total = http.getSize();
  if (total > (int)maxLen) {
    http.end();
    return false;
  }
  size_t cap = total > 0 ? total : maxLen;
  uint8_t* buf = (uint8_t*)malloc(cap);
  if (!buf) {
    http.end();
    return false;
  }
  WiFiClient* s = http.getStreamPtr();
  size_t n = 0;
  uint32_t deadline = millis() + 15000;
  while ((total < 0 || n < (size_t)total) && (int32_t)(millis() - deadline) < 0) {
    int avail = s->available();
    if (avail > 0) {
      if (n >= cap) break;  // too big
      int r = s->read(buf + n, std::min((size_t)avail, cap - n));
      if (r > 0) n += r;
    } else if (!s->connected()) {
      break;
    } else {
      delay(3);
    }
  }
  http.end();
  bool ok = n > 0 && (total < 0 ? n < cap : n == (size_t)total);
  if (!ok) {
    free(buf);
    return false;
  }
  *out = buf;
  *outLen = n;
  return true;
}

void copyStr(char* dst, size_t cap, const char* src) {
  if (!src) src = "";
  strncpy(dst, src, cap - 1);
  dst[cap - 1] = 0;
  // trim trailing spaces
  size_t n = strlen(dst);
  while (n > 0 && dst[n - 1] == ' ') dst[--n] = 0;
}

// ---------------------------------------------------------------------------
// Wi-Fi
// ---------------------------------------------------------------------------
void setWifiState(uint8_t st) {
  Lock l;
  g_net.wifi = st;
  if (st == W_CONNECTED) {
    strncpy(g_net.ip, WiFi.localIP().toString().c_str(), sizeof(g_net.ip) - 1);
    g_net.rssi = WiFi.RSSI();
  }
}

volatile bool s_pause = false;
volatile bool s_discEvent = false;
volatile uint8_t s_discReason = 0;
bool s_attempting = false;
uint32_t s_attemptStart = 0, s_failedAt = 0;

uint8_t classifyReason(uint8_t r) {
  switch (r) {
    case WIFI_REASON_AUTH_FAIL:              // 202
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: // 15: the usual "wrong WPA2 password"
    case WIFI_REASON_HANDSHAKE_TIMEOUT:      // 204
    case WIFI_REASON_MIC_FAILURE:            // 14
      return FAIL_PASSWORD;
    case WIFI_REASON_NO_AP_FOUND:            // 201
      return FAIL_NOT_FOUND;
    default:
      return (r >= 210 && r <= 212) ? FAIL_NOT_FOUND : FAIL_OTHER;
  }
}

void startConnect(bool userInitiated) {
  WiFi.disconnect(false, false);
  delay(50);
  s_discEvent = false;
  Serial.printf("wifi: connecting to '%s'\n", g_set.ssid);
  WiFi.begin(g_set.ssid, g_set.pass);
  s_attempting = true;
  s_attemptStart = millis();
  Lock l;
  if (userInitiated) g_net.failReason = FAIL_NONE;
  if (userInitiated || g_net.wifi != W_FAILED) g_net.wifi = W_CONNECTING;
}

void attemptFailed(uint8_t why) {
  s_attempting = false;
  s_failedAt = millis();
  WiFi.disconnect(false, false);  // stop the radio retrying so scans work
  Serial.printf("wifi: attempt failed (%s, reason %u)\n",
                why == FAIL_PASSWORD ? "wrong password" : why == FAIL_NOT_FOUND ? "network not found" : "timeout/other",
                (unsigned)s_discReason);
  Lock l;
  g_net.wifi = W_FAILED;
  g_net.failReason = why;
}

void handleWifi() {
  if (s_reconnect) {
    s_reconnect = false;
    if (!g_set.ssid[0]) {
      WiFi.disconnect(false, false);
      s_attempting = false;
      setWifiState(W_NO_CREDS);
      return;
    }
    startConnect(true);
    return;
  }
  if (!g_set.ssid[0]) {
    if (g_net.wifi != W_NO_CREDS) setWifiState(W_NO_CREDS);
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    s_attempting = false;
    if (g_net.wifi != W_CONNECTED) {
      setWifiState(W_CONNECTED);
      {
        Lock l;
        g_net.failReason = FAIL_NONE;
      }
      Serial.printf("wifi: connected, IP %s\n", g_net.ip);
      s_adsbNow = true;
      s_weatherNow = true;
    }
    if (millis() - s_lastRssi > 5000) {
      s_lastRssi = millis();
      Lock l;
      g_net.rssi = WiFi.RSSI();
    }
    return;
  }
  if (g_net.wifi == W_CONNECTED) {
    Serial.println("wifi: lost connection");
    startConnect(false);
    setWifiState(W_CONNECTING);
    return;
  }
  if (s_attempting) {
    if (s_discEvent) {
      s_discEvent = false;
      uint8_t why = classifyReason(s_discReason);
      if (why == FAIL_PASSWORD || why == FAIL_NOT_FOUND) return attemptFailed(why);
    }
    if (millis() - s_attemptStart > 20000) attemptFailed(FAIL_OTHER);
    return;
  }
  // Not connected and not trying: retry in the background, but never while
  // the Wi-Fi screens are open (a pending connection blocks network scans).
  if (g_net.wifi == W_FAILED) {
    if (s_pause) return;
    uint32_t wait = g_net.failReason == FAIL_PASSWORD ? 120000 : 20000;
    if (millis() - s_failedAt < wait) return;
  }
  startConnect(false);
}

void handleClock() {
  if (!s_ntpStarted) {
    configTime(0, 0, "pool.ntp.org", "time.google.com", "time.nist.gov");
    s_ntpStarted = true;
  }
  if (!g_net.timeSynced && time(nullptr) > 1700000000) {
    Lock l;
    g_net.timeSynced = true;
  }
}

// ---------------------------------------------------------------------------
// ADS-B
// ---------------------------------------------------------------------------
float readNum(JsonVariantConst v, float def) {
  return v.is<float>() || v.is<int>() ? v.as<float>() : def;
}

void doAdsb() {
  s_lastAdsb = millis();
  s_adsbNow = false;
  double lat = g_set.lat, lon = g_set.lon;
  float r = s_radiusNm;
  if (r < 3) r = 3;
  if (r > 250) r = 250;

  char url[160];
  switch (g_set.source) {
    case SRC_AIRPLANESLIVE:
      snprintf(url, sizeof(url), "https://api.airplanes.live/v2/point/%.4f/%.4f/%.0f", lat, lon, r);
      break;
    case SRC_ADSBLOL:
      snprintf(url, sizeof(url), "https://api.adsb.lol/v2/lat/%.4f/lon/%.4f/dist/%.0f", lat, lon, r);
      break;
    default:
      snprintf(url, sizeof(url), "https://opendata.adsb.fi/api/v3/lat/%.4f/lon/%.4f/dist/%.0f", lat, lon, r);
      break;
  }

  JsonDocument filter;
  JsonObject f = filter["ac"].add<JsonObject>();
  for (const char* k : {"hex", "flight", "r", "t", "desc", "lat", "lon", "track", "true_heading",
                        "gs", "alt_baro", "alt_geom", "squawk", "seen_pos", "dbFlags"}) {
    f[k] = true;
  }
  JsonDocument doc;
  int code = 0;
  bool ok = getJson(url, doc, &filter, &code, true);
  uint32_t now = millis();
  if (!ok && s_detailsReq) {  // interrupted by a tap; try again right after
    s_adsbNow = true;
    return;
  }
  if (!ok) {
    Serial.printf("adsb: failed (HTTP %d)\n", code);
    Lock l;
    g_adsb.lastHttp = code;
    return;
  }

  const float cosLat = cosf(lat * DEG_TO_RAD);
  float dist2[cfg::MAX_AIRCRAFT];
  int n = 0;
  for (JsonObjectConst p : doc["ac"].as<JsonArrayConst>()) {
    if (!p["lat"].is<float>() || !p["lon"].is<float>()) continue;
    Aircraft a{};
    const char* hex = p["hex"] | "";
    if (*hex == '~') hex++;
    a.hex = strtoul(hex, nullptr, 16);
    if (a.hex == 0) continue;
    a.lat = p["lat"];
    a.lon = p["lon"];
    copyStr(a.flight, sizeof(a.flight), p["flight"] | "");
    copyStr(a.reg, sizeof(a.reg), p["r"] | "");
    copyStr(a.type, sizeof(a.type), p["t"] | "");
    copyStr(a.desc, sizeof(a.desc), p["desc"] | "");
    a.track = readNum(p["track"], readNum(p["true_heading"], 0));
    a.gs = readNum(p["gs"], 0);
    JsonVariantConst ab = p["alt_baro"];
    if (ab.is<const char*>()) {
      a.ground = strcmp(ab.as<const char*>(), "ground") == 0;
      a.alt = a.ground ? 0 : ALT_UNKNOWN;
    } else if (ab.is<float>()) {
      a.alt = (int32_t)ab.as<float>();
    } else if (p["alt_geom"].is<float>()) {
      a.alt = (int32_t)p["alt_geom"].as<float>();
    } else {
      a.alt = ALT_UNKNOWN;
    }
    a.squawk = (uint16_t)atoi(p["squawk"] | "0");
    // Military: the feeds' database flag, plus the US military ICAO block as a backup.
    a.military = ((p["dbFlags"] | 0) & 1) || (a.hex >= 0xADF7C8 && a.hex <= 0xAFFFFF);
    float seen = readNum(p["seen_pos"], 0);
    if (seen < 0) seen = 0;
    if (seen > 30) seen = 30;
    a.posMs = now - (uint32_t)(seen * 1000);

    float dx = (a.lon - lon) * cosLat, dy = a.lat - lat;
    float d2 = dx * dx + dy * dy;
    if (n < cfg::MAX_AIRCRAFT) {
      s_work.list[n] = a;
      dist2[n++] = d2;
    } else {
      int far = 0;
      for (int i = 1; i < n; ++i) if (dist2[i] > dist2[far]) far = i;
      if (d2 < dist2[far]) {
        s_work.list[far] = a;
        dist2[far] = d2;
      }
    }
  }

  Lock l;
  memcpy(g_adsb.list, s_work.list, sizeof(Aircraft) * n);
  g_adsb.count = n;
  g_adsb.version++;
  g_adsb.okMs = now;
  g_adsb.lastHttp = 200;
}

// ---------------------------------------------------------------------------
// Weather (Open-Meteo) - also gives us the local UTC offset for the clock
// ---------------------------------------------------------------------------
void doWeather() {
  s_lastWeather = millis();
  s_weatherNow = false;
  {
    Lock l;
    g_weather.loading = true;
  }
  uint8_t units = g_set.units;
  const char* tu = units == U_METRIC ? "celsius" : "fahrenheit";
  const char* wu = units == U_METRIC ? "kmh" : units == U_AVIATION ? "kn" : "mph";
  char url[400];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m,wind_direction_10m,is_day"
           "&daily=temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=1"
           "&temperature_unit=%s&wind_speed_unit=%s",
           g_set.lat, g_set.lon, tu, wu);
  JsonDocument doc;
  int code = 0;
  uint32_t t0 = millis();
  bool ok = getJson(url, doc, nullptr, &code);
  Serial.printf("weather: %s (HTTP %d) in %lu ms\n", ok ? "ok" : "failed", code, (unsigned long)(millis() - t0));
  if (!ok) {
    Lock l;
    g_weather.loading = false;
    if (g_weather.fails < 255) g_weather.fails++;
    g_weather.lastHttp = code;
    // retry soon while we have nothing to show, more gently once we do
    uint32_t retry = g_weather.valid ? 60000 : 15000;
    s_lastWeather = millis() - cfg::WEATHER_INTERVAL_MS + retry;
    return;
  }
  JsonObjectConst cur = doc["current"];
  Lock l;
  g_weather.loading = false;
  g_weather.fails = 0;
  g_weather.lastHttp = code;
  g_weather.valid = true;
  g_weather.units = units;
  g_weather.temp = cur["temperature_2m"] | 0.0f;
  g_weather.humidity = cur["relative_humidity_2m"] | 0;
  g_weather.code = cur["weather_code"] | 0;
  g_weather.wind = cur["wind_speed_10m"] | 0.0f;
  g_weather.windDir = cur["wind_direction_10m"] | 0;
  g_weather.isDay = (cur["is_day"] | 1) != 0;
  g_weather.tmax = doc["daily"]["temperature_2m_max"][0] | g_weather.temp;
  g_weather.tmin = doc["daily"]["temperature_2m_min"][0] | g_weather.temp;
  g_weather.fetchedMs = millis();
  g_net.utcOffset = doc["utc_offset_seconds"] | 0;
  g_net.utcOffsetKnown = true;
}

// ---------------------------------------------------------------------------
// ZIP code -> lat/lon (zippopotam.us, US ZIP codes)
// ---------------------------------------------------------------------------
void doGeocode() {
  s_geoReq = false;
  char url[64];
  snprintf(url, sizeof(url), "http://api.zippopotam.us/us/%s", s_geoZip);
  JsonDocument doc;
  int code = 0;
  bool ok = getJson(url, doc, nullptr, &code);
  Lock l;
  if (!ok || doc["places"].size() == 0) {
    g_net.geo = (code == 404 || ok) ? G_NOT_FOUND : G_ERROR;
    return;
  }
  JsonObjectConst p = doc["places"][0];
  g_net.geoLat = atof(p["latitude"] | "0");
  g_net.geoLon = atof(p["longitude"] | "0");
  snprintf(g_net.geoPlace, sizeof(g_net.geoPlace), "%s, %s", (const char*)(p["place name"] | ""),
           (const char*)(p["state abbreviation"] | ""));
  g_net.geo = G_OK;
}

// ---------------------------------------------------------------------------
// Aircraft details: planespotters photo (via wsrv.nl) + adsbdb text.
// Results are cached, and the nearest planes are looked up in advance so a
// tap usually shows everything instantly.
//
// The photo request is the one the adsb.fi map (tar1090) makes:
//   api.planespotters.net/pub/photos/hex/{HEX}?reg={REG}&icaoType={TYPE}
// The map then loads photos[0].thumbnail.src straight into the browser. The
// board can't decode the progressive JPEGs that come back, so it asks wsrv.nl
// for a small baseline copy instead, and checks every file before keeping it.
//
// Only a definite answer is cached for long. A lookup that failed on the way
// (timeout, busy server, out of memory) is retried on the next tap, and the
// photo address is remembered so a retry goes straight to the download.
// ---------------------------------------------------------------------------
constexpr int CACHE_N = 10;
constexpr size_t CACHE_PHOTO_MAX = 8 * 1024;
constexpr uint32_t CACHE_TTL_MS = 60UL * 60 * 1000;
constexpr uint32_t CACHE_NONE_TTL_MS = 30UL * 60 * 1000;  // "no photo on file"
constexpr uint32_t CACHE_FAIL_TTL_MS = 2UL * 60 * 1000;   // something failed; prefetch retries after this
constexpr uint32_t PREFETCH_GAP_MS = 3000;
constexpr int PREFETCH_NEAREST = 6;

enum PhotoState : uint8_t {
  PH_UNKNOWN,  // not looked up yet
  PH_HAVE,     // bytes cached
  PH_URL,      // downloaded fine but too big to cache; address known
  PH_NONE,     // planespotters and adsbdb both have nothing
  PH_FAILED,   // a request failed; try again
};

struct CacheEntry {
  uint32_t hex = 0;
  uint32_t at = 0;
  bool textOk = false;
  uint8_t photoState = PH_UNKNOWN;
  bool photoSimilar = false;
  char reg[12] = "";
  char typeName[40] = "";
  char airline[40] = "";
  char route[32] = "";
  char photographer[32] = "";
  char photoUrl[160] = "";
  uint8_t* photo = nullptr;
  size_t photoLen = 0;
};
CacheEntry s_cache[CACHE_N];  // guarded by g_lock
uint32_t s_lastPrefetch = 0;

uint32_t cacheTtl(const CacheEntry& e) {
  if (!e.textOk || e.photoState == PH_FAILED || e.photoState == PH_UNKNOWN) return CACHE_FAIL_TTL_MS;
  if (e.photoState == PH_NONE) return CACHE_NONE_TTL_MS;
  return CACHE_TTL_MS;
}

// Caller holds g_lock.
CacheEntry* cacheFind(uint32_t hex) {
  for (auto& e : s_cache) {
    if (e.hex == hex && millis() - e.at < cacheTtl(e)) return &e;
  }
  return nullptr;
}

// Caller holds g_lock. Takes ownership of e.photo.
void cacheStore(CacheEntry& e) {
  CacheEntry* slot = nullptr;
  for (auto& c : s_cache) {
    if (c.hex == e.hex) { slot = &c; break; }
  }
  if (!slot) {
    slot = &s_cache[0];
    for (auto& c : s_cache) {
      if (c.hex == 0) { slot = &c; break; }
      if (c.at < slot->at) slot = &c;
    }
  }
  if (slot->photo) free(slot->photo);
  *slot = e;
  slot->at = millis();
  e.photo = nullptr;
}

// Caller holds g_lock. Copies whatever we know so far into the on-screen details.
void publish(const CacheEntry& e, bool textDone, bool photoDone) {
  if (g_details.hex != e.hex) return;
  if (e.reg[0]) copyStr(g_details.reg, sizeof(g_details.reg), e.reg);
  if (e.typeName[0]) copyStr(g_details.typeName, sizeof(g_details.typeName), e.typeName);
  if (e.airline[0]) copyStr(g_details.airline, sizeof(g_details.airline), e.airline);
  if (e.route[0]) copyStr(g_details.route, sizeof(g_details.route), e.route);
  if (e.photo && !g_details.photoReady) {
    uint8_t* copy = (uint8_t*)malloc(e.photoLen);
    if (copy) {
      memcpy(copy, e.photo, e.photoLen);
      g_details.photo = copy;
      g_details.photoLen = e.photoLen;
      g_details.photoReady = true;
      g_details.photoSerial++;
      g_details.photoSimilar = e.photoSimilar;
      copyStr(g_details.photographer, sizeof(g_details.photographer), e.photographer);
    }
  }
  if (textDone) g_details.state = D_DONE;
  if (photoDone) {
    g_details.photoLoading = false;
    if (!g_details.photoReady) {
      g_details.photoNone = true;
      g_details.photoFailed = e.photoState != PH_NONE;
    }
  }
}

// Case-insensitive substring test ("n999ar" in a planespotters link).
bool containsNoCase(const char* hay, const char* needle) {
  size_t n = strlen(needle);
  if (!n) return true;
  for (; *hay; ++hay) {
    if (strncasecmp(hay, needle, n) == 0) return true;
  }
  return false;
}

void titleCase(char* s) {
  bool start = true;
  for (; *s; ++s) {
    if (isalpha((unsigned char)*s)) {
      *s = start ? toupper(*s) : tolower(*s);
      start = false;
    } else {
      start = (*s == ' ' || *s == '-' || *s == '/');
    }
  }
}

void urlEncode(char* out, size_t cap, const char* src) {
  static const char HEXD[] = "0123456789ABCDEF";
  size_t e = 0;
  for (const char* p = src; *p && e + 4 < cap; ++p) {
    unsigned char ch = (unsigned char)*p;
    if (isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') out[e++] = ch;
    else { out[e++] = '%'; out[e++] = HEXD[ch >> 4]; out[e++] = HEXD[ch & 15]; }
  }
  out[e] = 0;
}

// True for a JPEG the on-chip decoder can draw (baseline). Progressive,
// lossless and arithmetic-coded files, and anything that isn't a JPEG
// (PNG, WebP, an HTML error page), are rejected.
bool jpegDrawable(const uint8_t* p, size_t n) {
  if (n < 4 || p[0] != 0xFF || p[1] != 0xD8) return false;
  size_t i = 2;
  while (i + 4 <= n) {
    if (p[i] != 0xFF) return false;
    uint8_t m = p[i + 1];
    if (m == 0xFF) { ++i; continue; }  // fill byte
    if (m == 0x01 || (m >= 0xD0 && m <= 0xD7)) { i += 2; continue; }
    if (m == 0xC0 || m == 0xC1) return true;  // baseline / extended sequential
    if (m >= 0xC2 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC) return false;
    if (m == 0xDA || m == 0xD9) return false;  // image data before a frame header
    i += 2 + ((p[i + 2] << 8) | p[i + 3]);
  }
  return false;
}

// Worth another go: no connection, timeout, or the server was busy.
bool transient(int code) { return code <= 0 || code == 429 || code >= 500; }

// One download attempt; keeps the file only if it can be drawn.
bool tryDownload(const char* url, const char* label, uint8_t** buf, size_t* len, int* code) {
  uint32_t t0 = millis();
  *buf = nullptr;
  *len = 0;
  bool ok = getBytes(url, buf, len, cfg::PHOTO_MAX_BYTES, code);
  bool drawable = ok && jpegDrawable(*buf, *len);
  Serial.printf("photo: %s HTTP %d, %u bytes, %lu ms%s\n", label, *code, (unsigned)*len,
                (unsigned long)(millis() - t0), ok && !drawable ? " (not a baseline JPEG, skipped)" : "");
  if (ok && !drawable) {
    free(*buf);
    *buf = nullptr;
    *len = 0;
    return false;
  }
  return ok;
}

// Download a photo resized to the photo box by wsrv.nl (always a baseline
// JPEG); one retry if the first try failed on the way; then the original
// file, used only if it happens to be baseline.
bool fetchPhoto(const char* src, uint8_t** buf, size_t* len) {
  char enc[300];
  urlEncode(enc, sizeof(enc), src);
  char url[400];
  snprintf(url, sizeof(url), "https://wsrv.nl/?url=%s&w=%d&h=%d&fit=cover&output=jpg&q=80", enc, cfg::PHOTO_W,
           cfg::PHOTO_H);
  int code = 0;
  if (tryDownload(url, "resized", buf, len, &code)) return true;
  if (transient(code)) {
    delay(600);
    if (tryDownload(url, "resized (retry)", buf, len, &code)) return true;
  }
  return tryDownload(src, "direct", buf, len, &code);
}

// Looks one aircraft up. interactive = the user tapped it (publish as we go).
// A prefetch gives way as soon as the user taps a different plane.
void lookup(const Aircraft& ac, bool interactive) {
  uint32_t t0 = millis();
  CacheEntry e;
  {
    // pick up what an earlier, partly failed lookup already found
    Lock l;
    if (const CacheEntry* c = cacheFind(ac.hex)) {
      e = *c;
      e.photo = nullptr;  // the cache keeps its own copy
      e.photoLen = 0;
      if (c->photo) {
        e.photo = (uint8_t*)malloc(c->photoLen);
        if (e.photo) {
          memcpy(e.photo, c->photo, c->photoLen);
          e.photoLen = c->photoLen;
        }
      }
    }
  }
  e.hex = ac.hex;
  if (!e.reg[0]) copyStr(e.reg, sizeof(e.reg), ac.reg);
  char hex[8];
  snprintf(hex, sizeof(hex), "%06X", (unsigned)ac.hex);

  // true if we should stop; also turns a prefetch into the tapped lookup
  auto interrupted = [&]() -> bool {
    if (!s_detailsReq) return false;
    Lock l;
    if (s_detailsAc.hex == ac.hex) {
      s_detailsReq = false;
      interactive = true;
      publish(e, e.textOk, false);
      return false;
    }
    return true;
  };
  auto abandon = [&]() {
    if (e.photo) free(e.photo);
  };

  // 1. photo (shown first; it's what people look at)
  bool apiAnswered = false;  // planespotters gave a definite answer (photo or none)
  if (!e.photo) {
    char url[200];
    if (!e.photoUrl[0]) {
      auto planespotters = [&](const char* u) {
        JsonDocument pf;
        pf["photos"][0]["thumbnail"] = true;  // {"src":…} on planespotters, a plain string on look-alikes
        pf["photos"][0]["thumbnail_large"]["src"] = true;
        pf["photos"][0]["photographer"] = true;
        pf["photos"][0]["link"] = true;
        int code = 0;
        for (int attempt = 0; attempt < 2; ++attempt) {
          JsonDocument pd;
          if (getJson(u, pd, &pf, &code)) {
            apiAnswered = true;
            JsonVariantConst p0 = pd["photos"][0];
            JsonVariantConst th = p0["thumbnail"];
            const char* src = th.is<const char*>() ? th.as<const char*>() : (th["src"] | "");
            if (!*src) src = p0["thumbnail_large"]["src"] | "";
            copyStr(e.photoUrl, sizeof(e.photoUrl), src);
            copyStr(e.photographer, sizeof(e.photographer), p0["photographer"] | "");
            // The photo page's address contains the pictured aircraft's registration.
            const char* link = p0["link"] | "";
            e.photoSimilar = e.photoUrl[0] && ac.reg[0] && *link && !containsNoCase(link, ac.reg);
            break;
          }
          Serial.printf("photo: planespotters HTTP %d%s\n", code, transient(code) && !attempt ? ", retrying" : "");
          if (!transient(code)) break;
          delay(800);
        }
        Serial.printf("photo: planespotters -> %s%s\n", e.photoUrl[0] ? e.photoUrl : "(none)",
                      e.photoSimilar ? " (same type, different aircraft)" : "");
      };
      char reg[32] = "", type[16] = "", q[64] = "";
      urlEncode(reg, sizeof(reg), ac.reg);
      urlEncode(type, sizeof(type), ac.type);
      if (reg[0] && type[0]) snprintf(q, sizeof(q), "?reg=%s&icaoType=%s", reg, type);
      else if (reg[0]) snprintf(q, sizeof(q), "?reg=%s", reg);
      else if (type[0]) snprintf(q, sizeof(q), "?icaoType=%s", type);
      snprintf(url, sizeof(url), "https://api.planespotters.net/pub/photos/hex/%s%s", hex, q);
      planespotters(url);
      if (interrupted()) return abandon();
      if (!e.photoUrl[0] && reg[0]) {
        bool first = apiAnswered;
        apiAnswered = false;
        snprintf(url, sizeof(url), "https://api.planespotters.net/pub/photos/reg/%s", reg);
        planespotters(url);
        apiAnswered = apiAnswered && first;
        if (interrupted()) return abandon();
      }
    }
    if (e.photoUrl[0]) {
      if (fetchPhoto(e.photoUrl, &e.photo, &e.photoLen)) e.photoState = PH_HAVE;
      if (interactive) {
        Lock l;
        publish(e, e.textOk, false);
      }
      if (interrupted()) return abandon();
    }
  }

  // 2. text: type, owner/airline, route
  char fallbackPhoto[160] = "";
  if (!e.textOk || !e.photo) {  // adsbdb also has the backup photo
    char url[200];
    if (ac.flight[0]) snprintf(url, sizeof(url), "https://api.adsbdb.com/v0/aircraft/%s?callsign=%s", hex, ac.flight);
    else snprintf(url, sizeof(url), "https://api.adsbdb.com/v0/aircraft/%s", hex);
    JsonDocument filter;
    JsonObject fa = filter["response"]["aircraft"].to<JsonObject>();
    for (const char* k : {"type", "manufacturer", "registration", "registered_owner", "url_photo_thumbnail"}) fa[k] = true;
    JsonObject fr = filter["response"]["flightroute"].to<JsonObject>();
    fr["airline"]["name"] = true;
    for (const char* side : {"origin", "destination"}) {
      fr[side]["iata_code"] = true;
      fr[side]["icao_code"] = true;
    }
    JsonDocument doc;
    int code = 0;
    if (getJson(url, doc, &filter, &code)) {
      e.textOk = true;
      JsonObjectConst a = doc["response"]["aircraft"];
      JsonObjectConst rt = doc["response"]["flightroute"];
      const char* manu = a["manufacturer"] | "";
      const char* type = a["type"] | "";
      if (*type) {
        if (*manu && strncasecmp(type, manu, strlen(manu)) != 0) snprintf(e.typeName, sizeof(e.typeName), "%s %s", manu, type);
        else copyStr(e.typeName, sizeof(e.typeName), type);
      }
      const char* reg = a["registration"] | "";
      if (*reg) copyStr(e.reg, sizeof(e.reg), reg);
      const char* airline = rt["airline"]["name"] | (const char*)(a["registered_owner"] | "");
      copyStr(e.airline, sizeof(e.airline), airline);
      const char* o = rt["origin"]["iata_code"] | (const char*)(rt["origin"]["icao_code"] | "");
      const char* d = rt["destination"]["iata_code"] | (const char*)(rt["destination"]["icao_code"] | "");
      if (*o && *d) snprintf(e.route, sizeof(e.route), "%s \xE2\x86\x92 %s", o, d);
      copyStr(fallbackPhoto, sizeof(fallbackPhoto), a["url_photo_thumbnail"] | "");
    } else if (code == 404) {
      e.textOk = true;  // adsbdb doesn't know this aircraft; that's an answer too
    }
    if (!e.typeName[0] && ac.desc[0]) {
      copyStr(e.typeName, sizeof(e.typeName), ac.desc);
      titleCase(e.typeName);
    }
    if (interactive) {
      Lock l;
      publish(e, true, false);
    }
    if (interrupted()) return abandon();
  }

  // 3. no planespotters photo? try airport-data.com's
  if (!e.photo && fallbackPhoto[0] && strcmp(fallbackPhoto, e.photoUrl) != 0) {
    uint8_t* buf = nullptr;
    size_t len = 0;
    if (fetchPhoto(fallbackPhoto, &buf, &len)) {
      e.photo = buf;
      e.photoLen = len;
      e.photoState = PH_HAVE;
      e.photoSimilar = false;
      copyStr(e.photoUrl, sizeof(e.photoUrl), fallbackPhoto);
      copyStr(e.photographer, sizeof(e.photographer), "airport-data.com");
    }
  }

  if (!e.photo) {
    // "none" only when planespotters answered and adsbdb had nothing either
    e.photoState = (!e.photoUrl[0] && apiAnswered && e.textOk && !fallbackPhoto[0]) ? PH_NONE : PH_FAILED;
  }

  Lock l;
  if (interactive) publish(e, true, true);
  if (e.photo && e.photoLen > CACHE_PHOTO_MAX) {  // too big to keep; the address is enough to get it again
    free(e.photo);
    e.photo = nullptr;
    e.photoLen = 0;
    e.photoState = PH_URL;
  }
  cacheStore(e);
  static const char* const STATE[] = {"?", "photo", "photo (not cached)", "no photo on file", "photo failed"};
  Serial.printf("details %s: %s, %s, text %s, %lu ms\n", hex, interactive ? "tap" : "prefetch", STATE[e.photoState],
                e.textOk ? "ok" : "failed", (unsigned long)(millis() - t0));
}

void doDetails() {
  Aircraft ac;
  {
    Lock l;
    ac = s_detailsAc;
    s_detailsReq = false;
  }
  lookup(ac, true);
}

// Look up the nearest planes ahead of time, one at a time.
bool doPrefetch() {
  s_lastPrefetch = millis();
  if (ESP.getFreeHeap() < 70000) return false;  // keep room for the TLS connections
  Aircraft pick{};
  bool found = false;
  {
    Lock l;
    const float cosLat = cosf(g_set.lat * DEG_TO_RAD);
    const float maxD = s_radiusNm / 1.45f / 60.0f;  // visible ring, in degrees
    int idx[PREFETCH_NEAREST];
    float dist[PREFETCH_NEAREST];
    int n = 0;
    for (int i = 0; i < g_adsb.count; ++i) {
      const Aircraft& a = g_adsb.list[i];
      if (a.ground) continue;
      float dx = (a.lon - g_set.lon) * cosLat, dy = a.lat - g_set.lat;
      float d = dx * dx + dy * dy;
      if (d > maxD * maxD) continue;
      if (n < PREFETCH_NEAREST) {
        idx[n] = i;
        dist[n++] = d;
      } else {
        int far = 0;
        for (int k = 1; k < n; ++k) if (dist[k] > dist[far]) far = k;
        if (d < dist[far]) { idx[far] = i; dist[far] = d; }
      }
    }
    // nearest first
    for (int pass = 0; pass < n && !found; ++pass) {
      int best = -1;
      for (int k = 0; k < n; ++k) {
        if (idx[k] >= 0 && (best < 0 || dist[k] < dist[best])) best = k;
      }
      if (best < 0) break;
      const Aircraft& a = g_adsb.list[idx[best]];
      idx[best] = -1;
      if (!cacheFind(a.hex)) {
        pick = a;
        found = true;
      }
    }
  }
  if (found) lookup(pick, false);
  return found;
}

// ---------------------------------------------------------------------------
void task(void*) {
  static WiFiClientSecure tls;
  static WiFiClient plain;
  tls.setInsecure();  // public read-only data; no certificate store on the device
  s_tls = &tls;
  s_plain = &plain;

  for (;;) {
    handleWifi();
    if (g_net.wifi == W_CONNECTED) {
      handleClock();
      uint32_t now = millis();
      if (s_geoReq) {
        doGeocode();
      } else if (s_detailsReq) {
        doDetails();
      } else if (g_set.locValid && (s_weatherNow || s_lastWeather == 0 ||
                                    now - s_lastWeather >= cfg::WEATHER_INTERVAL_MS)) {
        doWeather();  // one quick request every 10 min, so it goes ahead of the radar
      } else if (g_set.locValid && (s_adsbNow || now - s_lastAdsb >= cfg::ADSB_INTERVAL_MS)) {
        doAdsb();
        s_lastAdsb = millis();  // count the 5 s from the end, so a slow fetch can't hog the line
      } else if (g_set.locValid && now - s_lastPrefetch >= PREFETCH_GAP_MS) {
        doPrefetch();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(40));
  }
}

}  // namespace

void begin() {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);  // we handle retries (the built-in one loops on a wrong password)
  WiFi.onEvent([](arduino_event_id_t ev, arduino_event_info_t info) {
    if (ev == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
      s_discReason = info.wifi_sta_disconnected.reason;
      s_discEvent = true;
    }
  });
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("flight-radar");
  xTaskCreate(task, "net", 16384, nullptr, 1, nullptr);
}

void reconnect() { s_reconnect = true; }
void pauseRetries(bool pause) { s_pause = pause; }

void requestGeocode(const char* zip) {
  {
    Lock l;
    strncpy(s_geoZip, zip, sizeof(s_geoZip) - 1);
    g_net.geo = G_PENDING;
  }
  s_geoReq = true;
}

void refreshWeather() { s_weatherNow = true; }
void refreshAdsb() { s_adsbNow = true; }
void setFetchRadiusNm(float nm) { s_radiusNm = nm; }

void requestDetails(const Aircraft& ac) {
  Lock l;
  if (g_details.photo) free(g_details.photo);
  g_details = Details{};
  g_details.hex = ac.hex;
  g_details.state = D_LOADING;
  g_details.photoLoading = true;
  strncpy(g_details.reg, ac.reg, sizeof(g_details.reg) - 1);
  if (CacheEntry* e = cacheFind(ac.hex)) {  // already looked up: show what we have now
    bool photoFinal = e->photo || e->photoState == PH_NONE;
    publish(*e, e->textOk, photoFinal);
    if (photoFinal && e->textOk) return;
    // otherwise fetch the missing part (a cached photo address skips the API call)
  }
  s_detailsAc = ac;
  s_detailsReq = true;
}

bool localTime(struct tm& out) {
  if (!g_net.timeSynced || !g_net.utcOffsetKnown) return false;
  time_t t = time(nullptr) + g_net.utcOffset;
  gmtime_r(&t, &out);
  return true;
}

}  // namespace net
