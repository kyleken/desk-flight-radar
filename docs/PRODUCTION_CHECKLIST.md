# Production checklist

Run through this for every board before it goes in a gift box. About 5 minutes per board.

## What goes in each box

- [ ] Waveshare ESP32-C6-Touch-LCD-2.8, flashed with v1.0.10
- [ ] USB-C cable (a **data** cable also lets them update it later)
- [ ] USB power adapter (optional; any phone charger works)
- [ ] Printed quick-start card (`docs/QuickStart-Card.pdf`)
- [ ] Optional: a stand or case

> Note: a factory reset erases **all** settings, including screen rotation. If a stand puts the
> USB port on top, tell the recipient: *Settings → Display, units & sound → Rotate screen 180°*.

## Flash

- [ ] Flash `release/FlightRadar-v1.0.10.bin` at `0x0` with **Erase all** ticked
      (see [FLASHING.md](FLASHING.md))
- [ ] Press RESET

## Test (on your own Wi-Fi)

| # | Check | Pass when |
|---|---|---|
| 1 | Display | Welcome screen: dark navy background, white text, readable QR code. No rainbow fringes. |
| 2 | Touch | Tap **Pick a network on this screen**: the network list opens where you tapped. |
| 3 | Wi-Fi (on screen) | Join your network with the keyboard. Status card turns green: "Connected to …". |
| 4 | Wrong password (optional) | A deliberately wrong password shows "Wrong password" within a few seconds and the list still refreshes. |
| 5 | Location | Enter your ZIP: the radar opens and shows your town in the weather strip. |
| 6 | Aircraft | Within ~10 s, planes appear and the bottom-right chip shows a green dot + "adsb.fi". |
| 7 | Weather | Temperature and conditions appear; the weather dot is green. |
| 8 | Tap a plane | Photo (or "No photo" / "Photo failed"), tail number, type and countdown appear in the strip. |
| 9 | Zoom | **+** and **−** change the range in the top-left chip. |
| 10 | Brightness | Settings → Display → drag Brightness: the screen follows. |
| 11 | Chime (optional) | Turn **Alert chime** on: a two-note chime plays. Turn it back off. |
| 12 | Phone setup | Settings → **Set up from your phone**: scanning the QR code joins `FlightRadar-XXXX` and the setup page opens. |
| 13 | Battery (if fitted) | About shows a sensible battery voltage (3.3–4.2 V). |

## Reset for the recipient

- [ ] Hold **BOOT** for **5 seconds** (or About & reset → Factory reset, tap twice)
- [ ] The **Welcome to Flight Radar** screen appears
- [ ] Unplug and pack

## Record (optional)

| Board # | Flashed | Tests 1–10 | Reset | Recipient |
|---|---|---|---|---|
| 1 | ☐ | ☐ | ☐ | |
| 2 | ☐ | ☐ | ☐ | |
| 3 | ☐ | ☐ | ☐ | |
| 4 | ☐ | ☐ | ☐ | |
| 5 | ☐ | ☐ | ☐ | |
| 6 | ☐ | ☐ | ☐ | |
| 7 | ☐ | ☐ | ☐ | |
| 8 | ☐ | ☐ | ☐ | |
| 9 | ☐ | ☐ | ☐ | |
| 10 | ☐ | ☐ | ☐ | |
