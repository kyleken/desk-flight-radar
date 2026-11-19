# Flashing guide

How to put the Desk Flight Radar firmware on a **Waveshare ESP32-C6-Touch-LCD-2.8**.
The same file works on every board.

- Firmware file: `release/FlightRadar-v1.0.11.bin` (a complete image: bootloader, partition table
  and app)
- Flash address: **`0x0`**
- Time per board: about 2 minutes

---

## Option A: web browser (recommended, nothing to install)

**You need:** Chrome or Edge on a desktop computer (Windows, Mac or Linux) and a USB-C cable that
carries data. Many phone-charging cables are power-only, and the board won't appear at all with
one of those. Firefox and Safari don't support Web Serial, so they won't work.

1. **Plug in the board.** No driver is needed; the ESP32-C6 has USB built in. Close anything else
   that might be using its port (Arduino IDE, PlatformIO, a serial monitor).
2. **Open** <https://espressif.github.io/esptool-js/>. Leave the baud rate at its default
   (921600). Drop it to 115200 only if flashing fails partway.
3. **Click Connect** and choose the board's port. It's usually called
   "USB JTAG/serial debug unit" (Windows: a COM port; Mac: `cu.usbmodem…`). The log should say
   it found an ESP32-C6.
   - No port listed, or it won't connect? Hold **BOOT**, tap **RESET**, release **BOOT**, then
     click Connect again.
4. **Set up the Program section:**
   - Flash Address: **`0x0`** (not 0x1000 or 0x10000)
   - Choose File: `FlightRadar-v1.0.11.bin`. If extra file rows are listed, delete them.
   - Flash Mode, Flash Frequency, Flash Size: leave all three on **keep**. The file already
     contains the right values (16 MB flash); changing them can stop the board from booting.
   - **Erase all**: tick it the **first time** you flash a new board (clears Waveshare's demo
     firmware). Leave it unticked when updating a board that already runs the radar; your Wi-Fi,
     location and settings are then kept.
5. **Click Program.** It takes 30–60 seconds. Don't unplug or close the tab until the log
   says it's finished ("Leaving…" or "Hash of data verified").
6. **Press RESET** on the board (or unplug and replug it). A new board shows
   **Welcome to Flight Radar** with a QR code. Click **Disconnect** in the browser before
   plugging in the next board.

## Option B: command line (esptool)

```sh
pip install esptool
esptool --chip esp32c6 write-flash 0x0 release/FlightRadar-v1.0.11.bin
# first flash of a new board: erase first
esptool --chip esp32c6 erase-flash
```

Older esptool versions spell the commands `esptool.py … write_flash` / `erase_flash`.

## Option C: build and flash from source

See [DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md): `pio run -t upload`.

---

## Check the file first (optional)

`release/SHA256SUMS.txt` holds the file's SHA-256 checksum. To confirm the download isn't damaged:

```sh
# Mac / Linux
shasum -a 256 release/FlightRadar-v1.0.11.bin
# Windows (PowerShell)
Get-FileHash release\FlightRadar-v1.0.11.bin -Algorithm SHA256
```

The result should match the value in `SHA256SUMS.txt`.

## Troubleshooting

| Problem | Fix |
|---|---|
| No serial port appears | Use a data cable; try another USB port; enter download mode (hold BOOT, tap RESET, release BOOT). |
| "Permission denied" / port busy | Close Arduino IDE, PlatformIO, serial monitors or other browser tabs using the port. |
| Flashing stops partway | Lower the baud rate to 115200 and try again. |
| Black screen after flashing | The file was written at the wrong address. Erase, then flash again at `0x0`. |
| Board stuck in download mode | Press RESET (without holding BOOT). |
| Screen colors look wrong | You're running firmware older than v1.0.1. Flash v1.0.11. |
