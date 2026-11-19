# Third-party notices

The Desk Flight Radar firmware includes or is built with the following. Full license texts are
in [`third_party/`](third_party/).

## Compiled into the firmware

| Component | Use | License | Text |
|---|---|---|---|
| [Arduino-ESP32](https://github.com/espressif/arduino-esp32) 3.3.12 and ESP-IDF 5.5.5 (Espressif) | Core framework, Wi-Fi, HTTP, TLS | Apache-2.0 | `third_party/Apache-2.0.txt` |
| [LovyanGFX](https://github.com/lovyan03/LovyanGFX) 1.2.36 (lovyan03; includes Adafruit GFX, TFT_eSPI and TJpgDec code) | Display driver, graphics, JPEG decoding, QR codes | MIT and BSD-2-Clause | `third_party/LovyanGFX-LICENSE.txt` |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) 7.4.3 (Benoît Blanchon) | JSON parsing | MIT | `third_party/ArduinoJson-LICENSE.txt` |
| [Inter](https://github.com/rsms/inter) 4.1 (Rasmus Andersson) | UI font (converted to bitmaps in `src/fonts_data.h`) | SIL Open Font License 1.1 | `third_party/Inter-OFL.txt` |
| [JetBrains Mono](https://github.com/JetBrains/JetBrainsMono) 2.304 | Numbers and labels font | SIL Open Font License 1.1 | `third_party/JetBrainsMono-OFL.txt` |
| Large-airport runway table from [MatixYo/ESp32-Plane-Radar](https://github.com/MatixYo/ESp32-Plane-Radar) (`src/airports.*`), built from [OurAirports](https://ourairports.com) public-domain data | Runway overlay | MIT | `third_party/ESp32-Plane-Radar-LICENSE.txt` |

## Adapted code

| Source | What was adapted | License |
|---|---|---|
| [Waveshare ESP32-C6-Touch-LCD-2.8 examples](https://github.com/waveshareteam/ESP32-C6-Touch-LCD-2.8) | ST7789 init sequence (modified: byte order), CH32V003 expander and CST3530 touch register protocol | Apache-2.0 |
| Espressif [esp_codec_dev](https://github.com/espressif/esp-adf/tree/master/components/esp_codec_dev) ES8311 driver | Codec register sequence in `src/audio.cpp` | Apache-2.0 |

## Online services used at runtime

These are not distributed with the firmware. The device queries them and credits them on its
About screen.

| Service | Data | Terms (summary) |
|---|---|---|
| [adsb.fi](https://adsb.fi) open data (default), [airplanes.live](https://airplanes.live), [adsb.lol](https://adsb.lol) | Live aircraft positions | Free for personal, non-commercial use, about 1 request per second; attribution requested |
| [Open-Meteo](https://open-meteo.com) | Weather and local time zone | Free for non-commercial use; data CC BY 4.0 |
| [adsbdb.com](https://www.adsbdb.com) | Aircraft type, owner, route | Free public API |
| [planespotters.net](https://www.planespotters.net) | Aircraft photos | Photographer credit shown on screen |
| [airport-data.com](https://www.airport-data.com) | Fallback aircraft photos | Credited on screen |
| [wsrv.nl](https://wsrv.nl) | Resizes airport-data.com photos for the device (planespotters photos are loaded unchanged) | Free image cache and resize service |
| [zippopotam.us](https://zippopotam.us) | US ZIP code to coordinates | Free public API |

Inspired by [MatixYo/ESp32-Plane-Radar](https://github.com/MatixYo/ESp32-Plane-Radar).
