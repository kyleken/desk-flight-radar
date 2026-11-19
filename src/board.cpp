// Board support for the Waveshare ESP32-C6-Touch-LCD-2.8.
// Register protocol follows Waveshare's own board_config library
// (Board_IO.cpp and Touch_CST3530.cpp in waveshareteam/ESP32-C6-Touch-LCD-2.8).
#include "board.h"
#include <Wire.h>
#include "config.h"

namespace board {
namespace {

uint8_t s_outputs = 0;
uint8_t s_direction = 0;
bool s_ok = false;

bool expWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(cfg::I2C_EXPANDER);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool expRead(uint8_t reg, uint8_t* buf, size_t len) {
  Wire.beginTransmission(cfg::I2C_EXPANDER);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)cfg::I2C_EXPANDER, (uint8_t)len) != len) return false;
  for (size_t i = 0; i < len; ++i) buf[i] = Wire.read();
  return true;
}

bool expSet(uint8_t pin, bool level) {
  if (!(s_direction & (1u << pin))) {
    s_direction |= (1u << pin);
    expWrite(cfg::EXP_REG_MODE, s_direction);
  }
  if (level) s_outputs |= (1u << pin);
  else s_outputs &= ~(1u << pin);
  return expWrite(cfg::EXP_REG_OUT, s_outputs);
}

// --- CST3530 touch (32-bit register addresses) ---
constexpr uint32_t TP_DATA_REG = 0xD0070000;
constexpr uint32_t TP_END_REG  = 0xD00002AB;

void tpAddr(uint32_t reg) {
  Wire.write((uint8_t)(reg >> 24));
  Wire.write((uint8_t)(reg >> 16));
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)reg);
}

bool tpRead(uint32_t reg, uint8_t* buf, size_t len) {
  Wire.beginTransmission(cfg::I2C_TOUCH);
  tpAddr(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)cfg::I2C_TOUCH, (uint8_t)len) != len) return false;
  for (size_t i = 0; i < len; ++i) buf[i] = Wire.read();
  return true;
}

void tpEndRead() {
  Wire.beginTransmission(cfg::I2C_TOUCH);
  tpAddr(TP_END_REG);
  Wire.endTransmission(true);
}

}  // namespace

bool begin() {
  pinMode(cfg::PIN_SD_CS, OUTPUT);
  digitalWrite(cfg::PIN_SD_CS, HIGH);
  pinMode(cfg::PIN_TP_INT, INPUT);

  Wire.begin(cfg::PIN_I2C_SDA, cfg::PIN_I2C_SCL, 400000);
  Wire.setTimeOut(20);

  // Reset lines released, amplifier off; leave the sensor pins (EXIO4/5) alone.
  s_direction = (1u << cfg::EXIO_TP_RST) | (1u << cfg::EXIO_LCD_RST) | (1u << cfg::EXIO_PA_EN);
  s_outputs = (1u << cfg::EXIO_TP_RST) | (1u << cfg::EXIO_LCD_RST);
  s_ok = expWrite(cfg::EXP_REG_OUT, s_outputs) && expWrite(cfg::EXP_REG_MODE, s_direction);
  if (!s_ok) {
    delay(50);  // CH32 may still be booting
    s_ok = expWrite(cfg::EXP_REG_OUT, s_outputs) && expWrite(cfg::EXP_REG_MODE, s_direction);
  }
  setBacklight(0);
  resetLcd();

  // Touch reset
  expSet(cfg::EXIO_TP_RST, false);
  delay(100);
  expSet(cfg::EXIO_TP_RST, true);
  delay(300);
  return s_ok;
}

void resetLcd() {
  expSet(cfg::EXIO_LCD_RST, false);
  delay(50);
  expSet(cfg::EXIO_LCD_RST, true);
  delay(120);
}

void setBacklight(uint8_t percent) {
  if (percent > 100) percent = 100;
  expWrite(cfg::EXP_REG_PWM, (uint8_t)(percent * 255 / 100));
}

void setAmp(bool on) { expSet(cfg::EXIO_PA_EN, on); }

bool readBattery(float& volts) {
  uint32_t sum = 0;
  for (int i = 0; i < 8; ++i) {
    uint8_t d[2];
    if (!expRead(cfg::EXP_REG_ADC, d, 2)) return false;
    uint16_t v = d[0] | (uint16_t(d[1]) << 8);
    if (v > 1023) return false;
    sum += v;
    delay(2);
  }
  volts = (sum / 8.0f) * 3.3f / 1023.0f * 3.0f;  // 200k/100k divider
  return true;
}

bool readTouch(int16_t& x, int16_t& y) {
  uint8_t buf[9];
  if (!tpRead(TP_DATA_REG, buf, sizeof(buf))) return false;
  uint8_t count = buf[3] & 0x0F;
  bool down = count > 0 && count <= 5 && (buf[8] & 0xF0) != 0;
  tpEndRead();
  if (!down) return false;
  x = ((buf[7] & 0x0F) << 8) | buf[4];
  y = ((buf[7] & 0xF0) << 4) | buf[5];
  if (x >= cfg::SCREEN_W) x = cfg::SCREEN_W - 1;
  if (y >= cfg::SCREEN_H) y = cfg::SCREEN_H - 1;
  return true;
}

}  // namespace board
