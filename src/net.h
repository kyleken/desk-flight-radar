// Background network task: Wi-Fi, clock, ADS-B, weather, lookups
#pragma once
#include <Arduino.h>
#include "state.h"

namespace net {

void begin();

// UI -> network requests (non-blocking)
void reconnect();                                   // Wi-Fi credentials changed
void pauseRetries(bool pause);                      // Wi-Fi screens open: no background retries
void requestGeocode(const char* zip);               // ZIP -> lat/lon
void refreshWeather();                              // location or units changed
void refreshAdsb();                                 // range or source changed
void requestDetails(const Aircraft& ac);            // aircraft tapped
void releasePhoto();                                // details popup closed
void setFetchRadiusNm(float nm);                    // what the radar needs

// Local time helpers (valid once NTP + time zone are known)
bool localTime(struct tm& out);

}  // namespace net
