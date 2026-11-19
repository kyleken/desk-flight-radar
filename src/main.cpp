// Desk Flight Radar
// Waveshare ESP32-C6-Touch-LCD-2.8: live ADS-B radar (43.2 mm square) + weather.
#include <Arduino.h>
#include "audio.h"
#include "board.h"
#include "config.h"
#include "display.h"
#include "net.h"
#include "settings.h"
#include "state.h"
#include "ui.h"

static uint32_t s_bootDownSince = 0;

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println("\nDesk Flight Radar v" FW_VERSION);

  stateInit();
  settingsLoad();
  pinMode(cfg::PIN_BOOT, INPUT_PULLUP);

  if (!board::begin()) Serial.println("board: I/O expander not responding");
  display::begin(g_set.flip);
  net::begin();         // Wi-Fi in station mode; the setup screen may add the hotspot
  ui::begin();          // draws the first screen, then turns the backlight on
  audio::begin();
  Serial.printf("heap free: %u bytes\n", (unsigned)ESP.getFreeHeap());
}

void loop() {
  ui::loop();

  // Hold the BOOT button for 5 seconds to erase all settings (e.g. before gifting).
  if (digitalRead(cfg::PIN_BOOT) == LOW) {
    if (!s_bootDownSince) s_bootDownSince = millis();
    if (millis() - s_bootDownSince > 5000) {
      board::setBacklight(0);
      settingsFactoryReset();
    }
  } else {
    s_bootDownSince = 0;
  }
  delay(2);
}
