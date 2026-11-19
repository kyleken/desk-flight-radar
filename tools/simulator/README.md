# Screen simulator

Compiles the firmware's real UI code (`src/ui.cpp`, every `screen_*.cpp`, `icons.cpp`, fonts,
settings) for Linux and renders each screen to a PNG. Use it to check layouts without
flashing a board.

Hardware and network code are replaced by stand-ins:

| File | Stands in for |
|---|---|
| `stubs/` | Arduino, Wi-Fi, Preferences (NVS) and FreeRTOS headers |
| `overrides/stubs_hw.cpp` | `board`, `net`, `portal`, `audio`: fixed time, fake Wi-Fi scan, no network |
| `gen_display.py` | Turns the LCD into an in-memory 240×320 sprite (the rest of `display.*` is unchanged) |
| `overrides/sim_main.cpp` | The script: loads sample aircraft and weather, taps planes, opens each screen, saves frames |
| `sample_photo.jpg` | Stand-in aircraft photo (exercises the real JPEG decoder) |

## Run

Needs Linux or WSL with `g++`, `gcc`, `python3` and Pillow (`pip install pillow`), and one
`pio run` in the project first (it downloads the LovyanGFX sources).

```sh
sh tools/simulator/build.sh
```

The first run also builds LovyanGFX for the host (about a minute). Output:
`tools/simulator/out/*.png` at real size (240×320) plus `contact_sheet.png` at 2×.

## Add a screen or scenario

Edit `overrides/sim_main.cpp`: set up `g_set`, `g_adsb`, `g_weather`, `g_details` or `g_net`,
navigate with `ui::go(...)` or call a screen's `onTap(x, y)`, then `shot("name")`.
Advance time with `g_simMillis += ms`. If the radar should reflect new positions before a tap,
call `ui::invalidate(); ui::loop();` first.
