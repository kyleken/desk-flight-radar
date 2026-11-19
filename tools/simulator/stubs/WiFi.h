#pragma once
#include "Arduino.h"
#define WIFI_SCAN_FAILED (-2)
#define WIFI_SCAN_RUNNING (-1)
enum { WIFI_AUTH_OPEN = 0, WIFI_AUTH_WPA2_PSK = 3 };
struct WiFiStub {
  int scanComplete();
  String SSID(int i);
  int RSSI(int i);
  int encryptionType(int i);
  int scanNetworks(bool = false) { return -1; }
  void scanDelete() {}
};
extern WiFiStub WiFi;
