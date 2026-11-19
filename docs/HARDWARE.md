# Hardware reference

**Board:** Waveshare ESP32-C6-Touch-LCD-2.8
(<https://docs.waveshare.com/ESP32-C6-Touch-LCD-2.8>; example code:
<https://github.com/waveshareteam/ESP32-C6-Touch-LCD-2.8>, Apache-2.0).

- ESP32-C6 (single-core RISC-V, 160 MHz, ~320 KB usable RAM, no PSRAM), 16 MB flash,
  Wi-Fi 6 **2.4 GHz only**, native USB (Serial/JTAG)
- 2.8" IPS, 240 × 320, ST7789 over SPI. Pixel pitch ≈ 0.18 mm, so the active area is
  43.2 × 57.6 mm and **the top 240 × 240 px is a 43.2 × 43.2 mm square** (the radar).
- Capacitive touch: CST3530
- Audio: ES8311 codec + speaker amplifier
- CH32V003 microcontroller acting as an I/O expander (resets, backlight, amp, battery ADC)

## Pins

| Function | GPIO |
|---|---|
| LCD SCLK | 0 |
| LCD MOSI | 1 |
| LCD DC | 10 |
| LCD CS | 11 |
| SD card CS (shares SPI; held high, unused) | 23 |
| I2C SDA | 6 |
| I2C SCL | 7 |
| Touch INT (not used; touch is polled) | 18 |
| I2S BCLK | 15 |
| I2S WS (LRCK) | 3 |
| I2S DOUT (to codec) | 2 |
| I2S DIN (from codec, unused) | 20 |
| BOOT button | 9 |

## I2C bus (400 kHz)

| Address | Device |
|---|---|
| 0x24 | CH32V003 I/O expander |
| 0x58 | CST3530 touch |
| 0x18 | ES8311 codec (0x30 in 8-bit form) |

## CH32V003 expander

| Register | Use |
|---|---|
| 0x02 | Pin direction (1 = output) |
| 0x03 | Output levels |
| 0x05 | Backlight PWM, 0–255 |
| 0x06 | Battery ADC, 2 bytes little-endian, 10-bit; V = raw × 3.3 / 1023 × 3 |

| Expander pin | Use |
|---|---|
| EXIO0 | Touch reset (active low) |
| EXIO1 | LCD reset (active low) |
| EXIO3 | Speaker amplifier enable |

## Display setup (important)

`src/display.cpp` uses Waveshare's ST7789 init sequence with **two changes**, both confirmed on
real hardware:

1. **RAMCTRL (0xB0) = `0x00, 0xE0`** (big-endian pixel data). Waveshare's value `0xE8` selects
   little-endian, which suits their own drivers but scrambles every colour with LovyanGFX.
2. **`rgb_order = true`**: the panel is BGR.

Inversion is on (`invert = true`). The SPI clock is 40 MHz.

## Touch (CST3530)

32-bit register addresses. Read 9 bytes from `0xD0070000`; a touch is present when
`(buf[3] & 0x0F)` is 1–5 and `(buf[8] & 0xF0) != 0`.
X = `((buf[7] & 0x0F) << 8) | buf[4]`, Y = `((buf[7] & 0xF0) << 4) | buf[5]`.
Then write `0xD00002AB` to end the read. Coordinates match the portrait screen directly.

## Audio (ES8311)

Slave mode, internal MCLK derived from BCLK (no MCLK pin), 24 kHz, 16-bit stereo I2S. The
register sequence is ported from Espressif's `esp_codec_dev` ES8311 driver (Apache-2.0). The
amplifier is switched on only while a chime plays.

## Power

USB-C 5 V. The board has a battery connector; the About screen shows its voltage.
