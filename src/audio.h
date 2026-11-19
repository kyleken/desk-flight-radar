// Alert chime through the onboard ES8311 codec + speaker amplifier
#pragma once
#include <Arduino.h>

namespace audio {

enum Chime : uint8_t { CHIME_OVERHEAD, CHIME_EMERGENCY, CHIME_TEST };

void begin();              // sets up the background player (codec starts on first use)
void play(Chime c);        // non-blocking

}  // namespace audio
