// ES8311 bring-up ported from Espressif's esp_codec_dev es8311 driver
// (slave mode, MCLK derived from BCLK, 24 kHz / 16-bit, DAC only).
#include "audio.h"
#include <Wire.h>
#include <ESP_I2S.h>
#include <freertos/queue.h>
#include "board.h"
#include "config.h"

namespace audio {
namespace {

constexpr uint32_t RATE = 24000;
QueueHandle_t s_queue = nullptr;
I2SClass s_i2s;
bool s_ready = false;

bool wr(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(cfg::I2C_CODEC);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

uint8_t rd(uint8_t reg) {
  Wire.beginTransmission(cfg::I2C_CODEC);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return 0;
  if (Wire.requestFrom((uint8_t)cfg::I2C_CODEC, (uint8_t)1) != 1) return 0;
  return Wire.read();
}

bool codecInit() {
  bool ok = true;
  // open()
  if (rd(0x0D) != 0xFA) ok &= wr(0x0D, 0xFA);
  wr(0x44, 0x08);
  ok &= wr(0x44, 0x08);
  ok &= wr(0x01, 0x30);
  ok &= wr(0x02, 0x00);
  ok &= wr(0x03, 0x10);
  ok &= wr(0x16, 0x24);
  ok &= wr(0x04, 0x10);
  ok &= wr(0x05, 0x00);
  ok &= wr(0x0B, 0x00);
  ok &= wr(0x0C, 0x00);
  ok &= wr(0x10, 0x1F);
  ok &= wr(0x11, 0x7F);
  ok &= wr(0x00, 0x80);
  ok &= wr(0x00, rd(0x00) & 0xBF);           // slave mode
  ok &= wr(0x01, 0xBF);                       // internal MCLK from SCLK (BCLK)
  ok &= wr(0x06, rd(0x06) & ~0x20);           // SCLK not inverted
  ok &= wr(0x13, 0x10);
  ok &= wr(0x1B, 0x0A);
  ok &= wr(0x1C, 0x6A);
  ok &= wr(0x44, 0x58);
  // set_fs(): 16-bit, standard I2S, 24 kHz (coeff row: mclk 6.144 MHz)
  ok &= wr(0x09, (rd(0x09) | 0x0C) & 0xFC);
  ok &= wr(0x0A, (rd(0x0A) | 0x0C) & 0xFC);
  ok &= wr(0x02, (rd(0x02) & 0x07) | (0 << 5) | (3 << 3));  // pre_div 1, x8 multiplier
  ok &= wr(0x05, 0x00);                                      // adc/dac div 1
  ok &= wr(0x03, (rd(0x03) & 0x80) | 0x10);                  // single speed, adc osr
  ok &= wr(0x04, (rd(0x04) & 0x80) | 0x10);                  // dac osr
  ok &= wr(0x07, (rd(0x07) & 0xC0) | 0x00);                  // lrck_h
  ok &= wr(0x08, 0xFF);                                      // lrck_l
  ok &= wr(0x06, (rd(0x06) & 0xE0) | (4 - 1));               // bclk div
  // start()
  ok &= wr(0x00, 0x80);
  ok &= wr(0x01, 0xBF);
  ok &= wr(0x09, rd(0x09) & 0xBF);            // DAC on
  ok &= wr(0x0A, rd(0x0A) & 0xBF);
  ok &= wr(0x17, 0xBF);
  ok &= wr(0x0E, 0x02);
  ok &= wr(0x12, 0x00);
  ok &= wr(0x14, 0x1A);
  ok &= wr(0x14, rd(0x14) & ~0x40);
  ok &= wr(0x0D, 0x01);
  ok &= wr(0x15, 0x40);
  ok &= wr(0x37, 0x08);
  ok &= wr(0x45, 0x00);
  ok &= wr(0x32, 0xB4);                       // DAC volume (~ -5.5 dB)
  ok &= wr(0x31, rd(0x31) & 0x9F);            // unmute
  return ok;
}

bool ensureReady() {
  if (s_ready) return true;
  s_i2s.setPins(cfg::PIN_I2S_BCLK, cfg::PIN_I2S_WS, cfg::PIN_I2S_DOUT, cfg::PIN_I2S_DIN, -1);
  if (!s_i2s.begin(I2S_MODE_STD, RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    Serial.println("audio: I2S start failed");
    return false;
  }
  delay(20);  // let BCLK run before clocking the codec
  if (!codecInit()) {
    Serial.println("audio: ES8311 not responding");
    return false;
  }
  s_ready = true;
  return true;
}

// Plays one bell-like tone (fundamental + soft overtone, exponential decay).
void tone(float freq, float seconds, float level) {
  static int16_t buf[256 * 2];
  const int total = (int)(RATE * seconds);
  const float w1 = 2.0f * PI * freq / RATE, w2 = 2.0f * PI * freq * 2.76f / RATE;
  const float decay = expf(-5.0f / total);
  float env = 1.0f;
  int i = 0;
  while (i < total) {
    int n = std::min(256, total - i);
    for (int k = 0; k < n; ++k, ++i) {
      float attack = i < 120 ? i / 120.0f : 1.0f;
      float v = (sinf(w1 * i) + 0.25f * sinf(w2 * i)) * env * attack * level;
      int16_t s = (int16_t)(v * 26000.0f);
      buf[k * 2] = s;
      buf[k * 2 + 1] = s;
      env *= decay;
    }
    s_i2s.write((uint8_t*)buf, n * 4);
  }
}

void silence(float seconds) {
  static int16_t zero[256 * 2] = {0};
  int total = (int)(RATE * seconds);
  while (total > 0) {
    int n = std::min(256, total);
    s_i2s.write((uint8_t*)zero, n * 4);
    total -= n;
  }
}

void task(void*) {
  uint8_t c;
  for (;;) {
    if (xQueueReceive(s_queue, &c, portMAX_DELAY) != pdTRUE) continue;
    if (!ensureReady()) continue;
    board::setAmp(true);
    silence(0.03f);
    if (c == CHIME_EMERGENCY) {
      for (int k = 0; k < 3; ++k) {
        tone(988, 0.18f, 0.8f);
        tone(740, 0.22f, 0.8f);
      }
    } else {
      tone(1319, 0.35f, 0.7f);   // E6 "ding"
      tone(988, 0.60f, 0.7f);    // B5 "dong"
    }
    silence(0.08f);
    board::setAmp(false);
  }
}

}  // namespace

void begin() {
  if (s_queue) return;
  s_queue = xQueueCreate(4, 1);
  xTaskCreate(task, "audio", 4096, nullptr, 1, nullptr);
}

void play(Chime c) {
  if (!s_queue) return;
  uint8_t v = c;
  xQueueSend(s_queue, &v, 0);
}

}  // namespace audio
