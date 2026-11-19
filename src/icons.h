// Small vector icons drawn with LovyanGFX primitives
#pragma once
#include "display.h"

namespace icons {

void chevronLeft(Canvas& c, int cx, int cy, int s, uint16_t col);
void chevronRight(Canvas& c, int cx, int cy, int s, uint16_t col);
void plus(Canvas& c, int cx, int cy, int s, uint16_t col);
void minus(Canvas& c, int cx, int cy, int s, uint16_t col);
void gear(Canvas& c, int cx, int cy, int r, uint16_t col, uint16_t bg);
void wifi(Canvas& c, int cx, int cy, int s, int level, uint16_t on, uint16_t off);
void pin(Canvas& c, int cx, int cy, int s, uint16_t col, uint16_t bg);
void radar(Canvas& c, int cx, int cy, int s, uint16_t col, uint16_t bg);
void sun(Canvas& c, int cx, int cy, int r, uint16_t col);
void info(Canvas& c, int cx, int cy, int r, uint16_t col, uint16_t bg);
void lock(Canvas& c, int cx, int cy, int s, uint16_t col, uint16_t bg);
void check(Canvas& c, int cx, int cy, int s, uint16_t col);
void backspace(Canvas& c, int cx, int cy, int s, uint16_t col, uint16_t bg);
void shift(Canvas& c, int cx, int cy, int s, uint16_t col);
void eye(Canvas& c, int cx, int cy, int s, uint16_t col, uint16_t bg);
void refresh(Canvas& c, int cx, int cy, int s, uint16_t col, uint16_t bg);
void phone(Canvas& c, int cx, int cy, int s, uint16_t col, uint16_t bg);
void star(Canvas& c, int cx, int cy, int r, uint16_t col);

// Aircraft silhouette, nose pointing along heading (degrees true)
void plane(Canvas& c, float x, float y, float heading, float size, uint16_t col);
// Aircraft side view for the "no photo" placeholder
void planeSide(Canvas& c, int x, int y, int w, uint16_t col);
// Santa's sleigh + reindeer, facing along heading
void sleigh(Canvas& c, float x, float y, float heading, uint16_t col, uint16_t accent);
void sleighArt(Canvas& c, int x, int y, int w, int h);

// Weather symbol for an Open-Meteo WMO code
void weather(Canvas& c, int cx, int cy, int code, bool day, uint16_t bg);

}  // namespace icons
