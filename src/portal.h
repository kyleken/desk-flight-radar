// Phone setup: the radar opens its own Wi-Fi hotspot with a captive web page.
#pragma once
#include <Arduino.h>

namespace portal {

struct Submission {
  char ssid[33];
  char pass[65];
  char zip[6];
  double lat, lon;
  bool hasLatLon;
};

void start();
void stop();
bool active();
void loop();                         // call often while active
const char* apName();                // e.g. FlightRadar-3F2A
bool take(Submission& out);          // true once per form submission

}  // namespace portal
