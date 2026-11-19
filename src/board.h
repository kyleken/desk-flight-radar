// Board support: CH32V003 I/O expander, CST3530 touch, backlight, battery
#pragma once
#include <Arduino.h>

namespace board {

// I2C bus, expander, LCD + touch reset. Call before display init.
bool begin();

// Pulse the LCD reset line (via expander).
void resetLcd();

// Backlight 0..100 %
void setBacklight(uint8_t percent);

// Speaker amplifier enable (via expander EXIO3)
void setAmp(bool on);

// Battery voltage (V). Returns false if unreadable. ~4.2 V full, USB alone reads ~4.2-5 V.
bool readBattery(float& volts);

// Touch: true while a finger is down. Coordinates in native 240x320 portrait.
bool readTouch(int16_t& x, int16_t& y);

}  // namespace board
