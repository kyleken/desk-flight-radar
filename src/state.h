// Data shared between the network task and the UI (guarded by one mutex)
#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config.h"

constexpr int32_t ALT_UNKNOWN = INT32_MIN;

struct Aircraft {
  uint32_t hex;
  char flight[9];      // callsign, trimmed
  char reg[11];        // registration / tail number
  char type[5];        // ICAO type code, e.g. B38M
  char desc[30];       // long type name if the feed has it
  float lat, lon;
  float track;         // degrees true
  float gs;            // knots
  int32_t alt;         // feet, ALT_UNKNOWN if missing
  uint32_t posMs;      // millis() at which lat/lon were valid
  uint16_t squawk;     // decimal digits as a number, e.g. 7700
  bool ground;
  bool military;       // feed dbFlags bit 0, or US military address block
};

struct AdsbData {
  Aircraft list[cfg::MAX_AIRCRAFT];
  int count = 0;
  uint32_t version = 0;      // bumps on every successful fetch
  uint32_t okMs = 0;         // millis() of last success
  int lastHttp = 0;
};

struct Weather {
  bool valid = false;
  uint8_t units = 0;         // units the numbers are in
  float temp = 0, tmax = 0, tmin = 0, wind = 0;
  int windDir = 0, humidity = 0, code = 0;
  bool isDay = true;
  uint32_t fetchedMs = 0;   // last success
  bool loading = false;     // request in flight
  uint8_t fails = 0;        // consecutive failures
  int lastHttp = 0;         // last HTTP status (negative = connection error)
};

enum DetailState : uint8_t { D_IDLE, D_LOADING, D_DONE };

struct Details {
  uint32_t hex = 0;
  uint8_t state = D_IDLE;
  char reg[12] = "";
  char typeName[40] = "";
  char airline[40] = "";
  char route[32] = "";
  char photographer[32] = "";
  bool photoLoading = false;
  bool photoReady = false;     // photo bytes available
  bool photoNone = false;      // no photo to show
  bool photoFailed = false;    // ...because a download failed (the next tap tries again)
  bool photoSimilar = false;   // photo is the same type, not this exact aircraft
  uint8_t* photo = nullptr;
  size_t photoLen = 0;
  uint32_t photoSerial = 0;    // bumps when a new photo arrives
};

enum WifiState : uint8_t { W_NO_CREDS, W_CONNECTING, W_CONNECTED, W_FAILED };
enum WifiFail : uint8_t { FAIL_NONE, FAIL_PASSWORD, FAIL_NOT_FOUND, FAIL_OTHER };
enum GeoState : uint8_t { G_IDLE, G_PENDING, G_OK, G_NOT_FOUND, G_ERROR };

struct NetStatus {
  uint8_t wifi = W_NO_CREDS;
  uint8_t failReason = FAIL_NONE;   // why the last attempt failed
  int rssi = 0;
  char ip[16] = "";
  bool timeSynced = false;
  int32_t utcOffset = 0;       // seconds, from the weather service (auto time zone)
  bool utcOffsetKnown = false;
  uint8_t geo = G_IDLE;
  double geoLat = 0, geoLon = 0;
  char geoPlace[40] = "";
};

extern SemaphoreHandle_t g_lock;
extern AdsbData g_adsb;
extern Weather g_weather;
extern Details g_details;
extern NetStatus g_net;

struct Lock {
  Lock() { xSemaphoreTake(g_lock, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(g_lock); }
};

void stateInit();
