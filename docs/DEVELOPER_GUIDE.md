# Developer guide

Everything needed to build, change and release the Desk Flight Radar firmware.
For pins and chips see [HARDWARE.md](HARDWARE.md).

---

## 1. Build environment

- [PlatformIO](https://platformio.org): the VS Code extension, or `pip install platformio`
- Platform: **pioarduino** `55.03.312-1` (Arduino-ESP32 3.3.12 / ESP-IDF 5.5.5). It's pinned in
  `platformio.ini`; the official PlatformIO espressif32 platform doesn't support Arduino 3.x on
  the ESP32-C6.
- Libraries (pinned by Git tag in `platformio.ini`): LovyanGFX 1.2.36, ArduinoJson 7.4.3.
  They're fetched from GitHub; the PlatformIO registry versions
  (`lovyan03/LovyanGFX@1.2.36`, `bblanchon/ArduinoJson@7.4.3`) work too.

```sh
pio run                  # build (first build downloads the toolchain, ~2 min)
pio run -t upload        # flash over USB
pio device monitor       # serial log, 115200 baud
```

Outputs in `.pio/build/radar/`:

| File | Use |
|---|---|
| `firmware.factory.bin` | Complete image for flashing at `0x0` (this is what gets released) |
| `firmware.bin` | App only, at `0x10000` |
| `firmware.elf` | Symbols, for decoding crash backtraces |

Build flags: USB CDC on boot (serial over the USB-C port), `CORE_DEBUG_LEVEL=1` (errors only),
and `-Wall -Wextra` on project sources. Partition table: `default_16MB.csv`.

## 2. Source layout

| File | Purpose |
|---|---|
| `main.cpp` | Startup order, BOOT-button factory reset |
| `config.h` | Version, pins, screen geometry, timings, limits |
| `theme.h` | Radar themes and UI colours (RGB565) |
| `board.*` | CH32V003 expander, CST3530 touch, backlight, amp, battery |
| `display.*` | ST7789 setup, smooth fonts, `Canvas` (band renderer) |
| `fonts_data.h` | Generated anti-aliased fonts (see §8) |
| `settings.*` | Settings struct, NVS storage, migration, option tables |
| `state.h` | Data shared between tasks (aircraft, weather, details, network status) |
| `net.*` | Network task: Wi-Fi, NTP, aircraft, weather, ZIP, aircraft details + photos, cache, prefetch |
| `portal.*` | Setup hotspot, DNS catch-all, web form |
| `audio.*` | ES8311 bring-up and chime player task |
| `icons.*` | Vector icons, plane shapes, weather symbols, sleigh |
| `ui.*` | Screen manager, touch handling, brightness/night logic, shared widgets |
| `screen_radar.cpp` | Radar + weather strip + details panel |
| `screen_settings.cpp` | Settings menu, Radar options, Display options, About |
| `screen_location.cpp` | ZIP / lat-long entry with keypad |
| `screen_wifi.cpp` | Network list, password keyboard, phone-setup (QR) screen |
| `screen_night.cpp` | Night dimming |
| `screen_military.cpp` | Military highlight colour |
| `airports.*` | Large-airport runway table (OurAirports, via MatixYo/ESp32-Plane-Radar) |

## 3. Architecture

Three FreeRTOS tasks share the single core (equal priority, time-sliced):

```
loop() task (UI)                  "net" task (16 KB stack)          "audio" task
- touch polling (15 ms)           - Wi-Fi state machine             - waits on a queue
- current screen tick + render    - NTP clock                       - plays chimes over I2S
- portal (web + DNS) servicing    - one job per pass, in priority:
- brightness / night dimming        ZIP lookup > tapped plane >
- all I2C except chime amp          weather > aircraft > prefetch
```

Shared data lives in `state.h` (`g_adsb`, `g_weather`, `g_details`, `g_net`) behind one
mutex (`Lock l;` RAII guard). The network task builds results in private buffers and copies
them in under the lock; the UI copies out what it needs.

### Rendering

The screen is drawn in **four bands of 240 × 80 px** through one 38 KB sprite
(`Canvas` in `display.h`). Each screen's `render(Canvas&)` is called once per band and draws
in normal screen coordinates; the canvas offsets everything by the band's `oy`. This avoids a
150 KB full-frame buffer while staying flicker-free. Use `c.visible(y, h)` to skip work outside
the current band. The radar is animated (redrawn every 200 ms); other screens redraw on change.

### Screens

Each screen derives from `ui::Screen` (`enter`, `leave`, `render`, `onDown`, `onDrag`,
`onTap`, `tick`, `animated`). Register a new one in `ui.h` (`ScreenId`, factory function) and
`ui::begin()`. Navigate with `ui::go(id)`. Widgets: `header`, `navRow`, `toggle`,
`segmented`/`segmentedHit`, `button`, `rowDivider`; `pressedIn()` gives press feedback.

### Aircraft pipeline

1. `doAdsb()` fetches `…/lat/{lat}/lon/{lon}/dist/{nm}` (radius = range × 1.45 to reach the
   square's corners), stream-parses with an ArduinoJson filter, keeps the 100 nearest. The next
   fetch is 5 s after this one **finishes**.
2. The radar copies the list when `g_adsb.version` changes, updates trails and alert checks.
3. Every frame, positions are dead-reckoned from `posMs` along track at ground speed
   (capped at 30 s), so planes move smoothly between fetches.
4. Military = `dbFlags & 1` from the feed, or ICAO address `ADF7C8–AFFFFF` (US military block).

### Tapped-plane details

Photos follow planespotters' photo API terms (planespotters.net/photo/api): a proper
User-Agent (`cfg::USER_AGENT`, with the project URL), photographer credit on screen, the image
loaded **unchanged from the address the API returns**, not proxied, not kept after it stops
being shown, and API answers cached for at most 24 h.

On a **tap**, the net task asks planespotters `/pub/photos/hex/{HEX}?reg=…&icaoType=…` (the
same request as the tar1090/adsb.fi map, which returns a same-type photo when there's no exact
match) and downloads `photos[0].thumbnail.src` straight from their CDN. The on-chip decoder only
draws **baseline** JPEGs, so every file goes through `jpegKind()` first. If planespotters has
nothing, or its file is progressive, or it can't be reached, the airport-data.com thumbnail that
adsbdb lists is used instead, resized by `wsrv.nl/?url=…&w=88&h=59&fit=cover&output=jpg`.
adsbdb `/v0/aircraft/{hex}?callsign=…` supplies type, owner and route.

If planespotters answers 403 or 429, it isn't asked again for 10 minutes. Downloads retry once
after a connection error, timeout or 5xx.

The 12-entry cache keeps **text and addresses only, never image bytes** (60 min; 2 min if the
text lookup failed). `releasePhoto()` frees the download, and the photo sprite is cleared, as
soon as the popup closes. When idle, the task looks up the **text** of the six nearest uncached
aircraft, one every 3 s, if free heap is above 70 KB; planespotters is never asked in advance.
A tap interrupts a running aircraft fetch, and a second tap cuts the previous lookup short.

### Wi-Fi

The built-in auto-reconnect is **off** (it loops forever on a wrong password, which blocks
scanning). `handleWifi()` reads the disconnect reason: wrong password (reasons 2xx/15/14) or
network-not-found ends the attempt immediately; otherwise an attempt times out after 20 s.
Background retries: 2 min after a wrong password, 20 s otherwise, and none while a Wi-Fi screen
is open (`net::pauseRetries`).

## 4. Settings and migration (read before adding a setting)

Settings are one `Settings` struct saved as a blob in NVS (namespace `radar`, key `cfg`).

To add a setting:

1. **Append** fields at the **end** of the struct, under a `// added in vX.Y.Z` comment.
   Never reorder or remove existing fields.
2. Make sure the struct's `sizeof` **changes** (add `spare` bytes if needed). New fields can fall
   into the old struct's tail padding; if the size didn't change, old saves would load those
   padding bytes as zeros instead of your defaults. (This happened with the night-dimming fields;
   `settingsLoad()` repairs it.)
3. Add a line to the migration ladder in `settingsLoad()` that recognizes the previous size and
   copies only up to `offsetof(Settings, <first new field>)`.
4. Set the default in `defaults()` and range-check it after loading.
5. Save in the screen's `leave()` (or right away for things like Wi-Fi).

Saved sizes so far: v1.0.0–1.0.6 = 192 bytes, v1.0.7+ = 200 bytes.

## 5. Timings and limits (`config.h`)

| Constant | Value |
|---|---|
| `ADSB_INTERVAL_MS` | 5000 (gap after each fetch; public APIs allow 1 req/s) |
| `WEATHER_INTERVAL_MS` | 10 min (15 s retry until first success, then 60 s) |
| `RADAR_FRAME_MS` | 200 |
| `STALE_AFTER_MS` | 60 s |
| `MAX_AIRCRAFT` | 100 |
| `TRAIL_POINTS` | 12 (60 s) |
| `PHOTO_MAX_BYTES` | 40 KB |
| Details cache / prefetch | 10 entries / nearest 6, every 3 s (`net.cpp`) |

Memory at build: ~100 KB static RAM of 320 KB; ~1.6 MB of the 6.5 MB app partition.

## 6. Serial log

Connect at 115200 baud (`pio device monitor`). Useful lines:

| Message | Meaning |
|---|---|
| `Desk Flight Radar v1.0.11`, `heap free: …` | Boot |
| `board: I/O expander not responding` | CH32V003 didn't answer (check I2C) |
| `wifi: connecting / connected, IP … / lost connection` | Wi-Fi state |
| `wifi: attempt failed (wrong password, reason 15)` | Why a connection failed |
| `adsb: failed (HTTP n)` | Aircraft fetch failed (negative n = connection error) |
| `weather: ok (HTTP 200) in 1450 ms` | Weather fetch |
| `photo: planespotters -> https://t.plnspttrs.net/…` | Photo lookup (`(none on file)`; `(same type, different aircraft)` for a stand-in) |
| `photo: planespotters refused the request (HTTP 403)` | Planespotters blocked the request; paused for 10 min |
| `photo: planespotters image HTTP 200, 9120 bytes, 600 ms` | Planespotters image, downloaded as is |
| `… (progressive JPEG, can't draw)` | File the decoder can't draw; the backup photo is tried |
| `photo: resized HTTP 200, 2843 bytes, 900 ms` | Backup (airport-data.com) photo via wsrv.nl (`direct` = original file) |
| `photo: decode failed (n bytes)` | Drawing failed on screen |
| `details A1B2C3: tap, photo, text ok, 2100 ms` | Result: `photo`, `no photo on file`, `photo failed` (`prefetch` = text only) |
| `portal: started FlightRadar-XXXX / stopped` | Setup hotspot |
| `audio: ES8311 not responding` | Codec didn't answer |

## 7. Screen simulator

`tools/simulator/` compiles the real UI code on Linux (or WSL) with stand-in hardware and
renders every screen to PNG, so layouts can be checked without a board. See its README.

## 8. Fonts

`src/fonts_data.h` is generated from the TTFs in `fonts/` by `tools/make_fonts.py`
(needs Python 3 + Pillow). Edit the `FONTS` list (name, file, pixel size) or the `EXTRA`
characters, then run `python3 tools/make_fonts.py`. Glyphs must stay **sorted by code point**:
LovyanGFX binary-searches them, and unsorted glyphs draw as a bar.

## 9. Releasing a new version

1. Bump `FW_VERSION` in `src/config.h`; add a `CHANGELOG.md` entry.
2. If you changed `Settings`, follow §4.
3. `pio run`; fix any new warnings.
4. Check layouts in the simulator if you touched the UI.
5. Copy `.pio/build/radar/firmware.factory.bin` to `release/FlightRadar-vX.Y.Z.bin`, remove the
   old one, and update `release/SHA256SUMS.txt` (`shasum -a 256 release/*.bin > release/SHA256SUMS.txt`).
6. Update version numbers in `README.md` and `docs/FLASHING.md`.
7. Flash one board with **Erase all unticked** to confirm settings survive the upgrade, then run
   the production checklist.

## 10. Known limitations

- 2.4 GHz Wi-Fi only; no captive-portal (sign-in page) networks.
- HTTPS certificates aren't checked (no certificate store on the device; all data is public and
  read-only).
- ZIP lookup covers US ZIP codes only (lat/long works anywhere).
- Route data from adsbdb can be missing or out of date for some callsigns.
- The setup hotspot is open (no password) while setup is active.
- Aircraft data services are free for personal, non-commercial use and ask for attribution
  (shown on the About screen).
