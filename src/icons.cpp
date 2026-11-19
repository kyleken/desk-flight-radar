#include "icons.h"
#include <math.h>
#include "theme.h"

namespace icons {
namespace {
constexpr float D2R = 0.01745329252f;

// LovyanGFX arc angles: 0 deg = 3 o'clock, increasing clockwise.
void arc(Canvas& c, int cx, int cy, int r, int thick, float a0, float a1, uint16_t col) {
  c.s->fillArc(cx, cy - c.oy, r, r - thick + 1, a0, a1, col);
}
}  // namespace

void chevronLeft(Canvas& c, int cx, int cy, int s, uint16_t col) {
  float h = s * 0.5f, w = s * 0.28f;
  c.wideLine(cx + w, cy - h, cx - w, cy, 1.1f, col);
  c.wideLine(cx - w, cy, cx + w, cy + h, 1.1f, col);
}

void chevronRight(Canvas& c, int cx, int cy, int s, uint16_t col) {
  float h = s * 0.5f, w = s * 0.28f;
  c.wideLine(cx - w, cy - h, cx + w, cy, 1.0f, col);
  c.wideLine(cx + w, cy, cx - w, cy + h, 1.0f, col);
}

void plus(Canvas& c, int cx, int cy, int s, uint16_t col) {
  float h = s * 0.5f;
  c.wideLine(cx - h, cy, cx + h, cy, 1.3f, col);
  c.wideLine(cx, cy - h, cx, cy + h, 1.3f, col);
}

void minus(Canvas& c, int cx, int cy, int s, uint16_t col) {
  float h = s * 0.5f;
  c.wideLine(cx - h, cy, cx + h, cy, 1.3f, col);
}

void gear(Canvas& c, int cx, int cy, int r, uint16_t col, uint16_t bg) {
  float body = r * 0.74f;
  for (int i = 0; i < 8; ++i) {
    float a = i * 45.0f * D2R;
    float w = 0.24f;
    float x0 = cx + cosf(a - w) * body, y0 = cy + sinf(a - w) * body;
    float x1 = cx + cosf(a + w) * body, y1 = cy + sinf(a + w) * body;
    float x2 = cx + cosf(a + w * 0.7f) * r, y2 = cy + sinf(a + w * 0.7f) * r;
    float x3 = cx + cosf(a - w * 0.7f) * r, y3 = cy + sinf(a - w * 0.7f) * r;
    c.tri(x0, y0, x1, y1, x2, y2, col);
    c.tri(x0, y0, x2, y2, x3, y3, col);
  }
  c.smoothCircle(cx, cy, (int)body, col);
  c.smoothCircle(cx, cy, (int)(r * 0.34f), bg);
}

void wifi(Canvas& c, int cx, int cy, int s, int level, uint16_t on, uint16_t off) {
  // cy is the bottom dot
  int r1 = s * 0.30f, r2 = s * 0.58f, r3 = s * 0.86f;
  arc(c, cx, cy, r3, 2, 225, 315, level >= 4 ? on : off);
  arc(c, cx, cy, r2, 2, 225, 315, level >= 3 ? on : off);
  arc(c, cx, cy, r1, 2, 225, 315, level >= 2 ? on : off);
  c.smoothCircle(cx, cy - 1, 1, on);
}

void pin(Canvas& c, int cx, int cy, int s, uint16_t col, uint16_t bg) {
  float r = s * 0.32f;
  float top = cy - s * 0.12f;
  c.smoothCircle(cx, top, (int)r, col);
  c.tri(cx - r * 0.86f, top + r * 0.5f, cx + r * 0.86f, top + r * 0.5f, cx, cy + s * 0.45f, col);
  c.smoothCircle(cx, top, (int)(r * 0.42f), bg);
}

void radar(Canvas& c, int cx, int cy, int s, uint16_t col, uint16_t bg) {
  int r = s / 2;
  c.ring(cx, cy, r, 1.6f, col);
  c.ring(cx, cy, r * 0.55f, 1.4f, col);
  c.smoothCircle(cx, cy, 2, col);
  c.wideLine(cx, cy, cx + r * 0.75f, cy - r * 0.75f, 0.9f, col);
}

void sun(Canvas& c, int cx, int cy, int r, uint16_t col) {
  c.smoothCircle(cx, cy, r, col);
  for (int i = 0; i < 8; ++i) {
    float a = i * 45 * D2R;
    c.wideLine(cx + cosf(a) * (r + 2.5f), cy + sinf(a) * (r + 2.5f), cx + cosf(a) * (r + 5),
               cy + sinf(a) * (r + 5), 0.8f, col);
  }
}

void info(Canvas& c, int cx, int cy, int r, uint16_t col, uint16_t bg) {
  c.ring(cx, cy, r, 1.6f, col);
  c.wideLine(cx, cy - 1, cx, cy + r * 0.45f, 0.9f, col);
  c.smoothCircle(cx, cy - r * 0.45f, 1, col);
}

void lock(Canvas& c, int cx, int cy, int s, uint16_t col, uint16_t bg) {
  int w = s * 0.7f, h = s * 0.5f;
  int top = cy - h / 2 + s * 0.15f;
  c.wideLine(cx - s * 0.22f, top, cx - s * 0.22f, cy - s * 0.2f, 0.8f, col);
  c.wideLine(cx + s * 0.22f, top, cx + s * 0.22f, cy - s * 0.2f, 0.8f, col);
  arc(c, cx, cy - s * 0.2f, s * 0.3f, 2, 180, 360, col);
  c.roundRect(cx - w / 2, top, w, h, 2, col);
}

void check(Canvas& c, int cx, int cy, int s, uint16_t col) {
  float h = s * 0.5f;
  c.wideLine(cx - h, cy, cx - h * 0.25f, cy + h * 0.7f, 1.2f, col);
  c.wideLine(cx - h * 0.25f, cy + h * 0.7f, cx + h, cy - h * 0.6f, 1.2f, col);
}

void backspace(Canvas& c, int cx, int cy, int s, uint16_t col, uint16_t bg) {
  float h = s * 0.36f, w = s * 0.5f;
  c.tri(cx - w, cy, cx - w * 0.4f, cy - h, cx - w * 0.4f, cy + h, col);
  c.fillRect(cx - w * 0.4f, cy - h, w * 1.4f, h * 2 + 1, col);
  float x = cx + w * 0.25f, d = h * 0.5f;
  c.wideLine(x - d, cy - d, x + d, cy + d, 0.9f, bg);
  c.wideLine(x - d, cy + d, x + d, cy - d, 0.9f, bg);
}

void shift(Canvas& c, int cx, int cy, int s, uint16_t col) {
  float h = s * 0.5f;
  c.tri(cx, cy - h, cx - h, cy, cx + h, cy, col);
  c.fillRect(cx - h * 0.45f, cy, h * 0.9f + 1, h * 0.9f, col);
}

void eye(Canvas& c, int cx, int cy, int s, uint16_t col, uint16_t bg) {
  float w = s * 0.5f, h = s * 0.3f;
  c.s->fillEllipse(cx, cy - c.oy, w, h, col);
  c.s->fillEllipse(cx, cy - c.oy, w - 1.5f, h - 1.5f, bg);
  c.smoothCircle(cx, cy, s * 0.16f, col);
}

void refresh(Canvas& c, int cx, int cy, int s, uint16_t col, uint16_t bg) {
  int r = s * 0.42f;
  arc(c, cx, cy, r, 2, 30, 320, col);
  float a = 320 * D2R;
  float tx = cx + cosf(a) * r, ty = cy + sinf(a) * r;
  c.tri(tx - 3, ty - 3, tx + 4, ty - 3, tx + 1, ty + 3, col);
}

void phone(Canvas& c, int cx, int cy, int s, uint16_t col, uint16_t bg) {
  int w = s * 0.5f, h = s * 0.86f;
  c.roundRect(cx - w / 2, cy - h / 2, w, h, 2, col);
  c.fillRect(cx - w / 2 + 2, cy - h / 2 + 3, w - 4, h - 7, bg);
  c.fillRect(cx - 1, cy + h / 2 - 3, 3, 1, bg);
}

void star(Canvas& c, int cx, int cy, int r, uint16_t col) {
  float ox[5], oy[5], ix[5], iy[5];
  for (int i = 0; i < 5; ++i) {
    float a = (-90 + i * 72) * D2R, b = (-90 + 36 + i * 72) * D2R;
    ox[i] = cx + cosf(a) * r;
    oy[i] = cy + sinf(a) * r;
    ix[i] = cx + cosf(b) * r * 0.42f;
    iy[i] = cy + sinf(b) * r * 0.42f;
  }
  for (int i = 0; i < 5; ++i) {
    int j = (i + 4) % 5;
    c.tri(ox[i], oy[i], ix[i], iy[i], ix[j], iy[j], col);
    c.tri(cx, cy, ix[i], iy[i], ix[j], iy[j], col);
  }
}

void plane(Canvas& c, float x, float y, float heading, float size, uint16_t col) {
  float a = heading * D2R;
  float rx = cosf(a), ry = sinf(a);      // right
  float bx = -sinf(a), by = cosf(a);     // toward tail
  auto P = [&](float u, float v, float& ox, float& oy) {
    ox = x + (u * rx + v * bx) * size;
    oy = y + (u * ry + v * by) * size;
  };
  float x0, y0, x1, y1, x2, y2, x3, y3;
  // fuselage
  P(0, -0.5f, x0, y0);
  P(0, 0.44f, x1, y1);
  c.wideLine(x0, y0, x1, y1, size * 0.075f + 0.4f, col);
  // wings
  for (int side = -1; side <= 1; side += 2) {
    P(0, -0.13f, x0, y0);
    P(side * 0.5f, 0.12f, x1, y1);
    P(side * 0.5f, 0.21f, x2, y2);
    P(0, 0.09f, x3, y3);
    c.tri(x0, y0, x1, y1, x2, y2, col);
    c.tri(x0, y0, x2, y2, x3, y3, col);
    // tailplane
    P(0, 0.30f, x0, y0);
    P(side * 0.21f, 0.43f, x1, y1);
    P(side * 0.21f, 0.50f, x2, y2);
    P(0, 0.45f, x3, y3);
    c.tri(x0, y0, x1, y1, x2, y2, col);
    c.tri(x0, y0, x2, y2, x3, y3, col);
  }
}

void planeSide(Canvas& c, int x, int y, int w, uint16_t col) {
  float s = w / 96.0f;
  auto X = [&](float v) { return x + v * s; };
  auto Y = [&](float v) { return y + v * s; };
  c.roundRect(X(4), Y(17), 82 * s, 8 * s, 4 * s, col);                 // fuselage
  c.tri(X(64), Y(17), X(78), Y(3), X(85), Y(3), col);                  // fin
  c.tri(X(64), Y(17), X(85), Y(3), X(80), Y(17), col);
  c.tri(X(40), Y(23), X(56), Y(23), X(44), Y(36), col);                // wing
  c.tri(X(44), Y(36), X(52), Y(36), X(56), Y(23), col);
}

void sleigh(Canvas& c, float x, float y, float heading, uint16_t col, uint16_t accent) {
  // Drawn facing right or left depending on the direction of travel.
  int dir = sinf(heading * D2R) >= 0 ? 1 : -1;
  // reindeer
  uint16_t brown = rgb(150, 100, 60);
  for (int i = 1; i <= 2; ++i) {
    float rx = x + dir * (7 + i * 6), ry = y - 1;
    c.fillRect(rx - 2, ry - 1, 5, 3, brown);
    c.fillRect(rx + dir * 2, ry - 3, 2, 2, brown);
    c.smoothLine(rx - 2, ry + 2, rx - 2, ry + 4, brown);
    c.smoothLine(rx + 2, ry + 2, rx + 2, ry + 4, brown);
  }
  c.fillCircle(x + dir * 22, y - 3, 1, rgb(255, 60, 60));  // Rudolph's nose
  c.smoothLine(x + dir * 4, y - 1, x + dir * 10, y - 1, accent);
  // sleigh
  c.roundRect(x - 6, y - 4, 11, 6, 2, col);
  c.smoothLine(x - 7, y + 3, x + 6, y + 3, accent);
  c.smoothLine(x + 6, y + 3, x + 7, y + 1, accent);
}

void sleighArt(Canvas& c, int x, int y, int w, int h) {
  c.fillRect(x, y, w, h, rgb(10, 22, 48));
  for (int i = 0; i < 14; ++i) {
    int sx = x + (i * 37) % w, sy = y + (i * 23) % (h - 10);
    c.fillRect(sx, sy, 1, 1, rgb(220, 230, 255));
  }
  c.smoothCircle(x + w - 16, y + 14, 9, rgb(245, 240, 210));
  c.smoothCircle(x + w - 12, y + 11, 8, rgb(10, 22, 48));
  float cx = x + w * 0.38f, cy = y + h * 0.62f;
  // sleigh
  c.roundRect(cx - 14, cy - 8, 24, 12, 4, rgb(210, 40, 50));
  c.wideLine(cx - 16, cy + 7, cx + 12, cy + 7, 1.0f, rgb(255, 215, 90));
  c.wideLine(cx + 12, cy + 7, cx + 15, cy + 3, 1.0f, rgb(255, 215, 90));
  c.smoothCircle(cx - 6, cy - 11, 4, rgb(210, 40, 50));            // Santa
  c.smoothCircle(cx - 6, cy - 16, 3, rgb(255, 220, 190));
  // reindeer
  for (int i = 0; i < 2; ++i) {
    float rx = cx + 22 + i * 14, ry = cy - 6 - i * 3;
    c.roundRect(rx - 5, ry - 2, 10, 5, 2, rgb(150, 100, 60));
    c.smoothLine(rx + 4, ry - 2, rx + 6, ry - 6, rgb(150, 100, 60));
    c.smoothLine(rx - 3, ry + 3, rx - 4, ry + 7, rgb(150, 100, 60));
    c.smoothLine(rx + 3, ry + 3, rx + 4, ry + 7, rgb(150, 100, 60));
  }
  c.smoothCircle(cx + 42, cy - 15, 1, rgb(255, 60, 60));
  c.smoothLine(cx + 10, cy - 4, cx + 17, cy - 6, rgb(255, 215, 90));
}

void weather(Canvas& c, int cx, int cy, int code, bool day, uint16_t bg) {
  const uint16_t SUN = rgb(255, 200, 97), CLOUD = rgb(220, 230, 240), CLOUD_D = rgb(150, 165, 180);
  const uint16_t RAIN = rgb(92, 180, 255), BOLT = rgb(255, 215, 90);
  auto cloud = [&](int x, int y, uint16_t col) {
    c.smoothCircle(x - 5, y + 2, 4, col);
    c.smoothCircle(x + 1, y - 1, 6, col);
    c.smoothCircle(x + 7, y + 2, 4, col);
    c.fillRect(x - 5, y + 2, 13, 5, col);
  };
  auto moon = [&](int x, int y, int r) {
    c.smoothCircle(x, y, r, rgb(240, 236, 210));
    c.smoothCircle(x + r * 0.55f, y - r * 0.45f, r * 0.85f, bg);
  };
  if (code == 0) {
    if (day) sun(c, cx, cy, 6, SUN);
    else moon(cx, cy, 8);
  } else if (code <= 2) {
    if (day) sun(c, cx - 5, cy - 5, 4, SUN);
    else moon(cx - 5, cy - 5, 6);
    cloud(cx + 2, cy + 2, CLOUD);
  } else if (code == 3) {
    cloud(cx - 3, cy - 3, CLOUD_D);
    cloud(cx + 2, cy + 2, CLOUD);
  } else if (code == 45 || code == 48) {
    for (int i = 0; i < 4; ++i) c.wideLine(cx - 9 + (i & 1) * 2, cy - 6 + i * 4, cx + 9, cy - 6 + i * 4, 0.9f, CLOUD);
  } else {
    bool snow = (code >= 71 && code <= 77) || code == 85 || code == 86;
    bool thunder = code >= 95;
    cloud(cx, cy - 4, CLOUD);
    if (thunder) {
      c.tri(cx + 1, cy + 3, cx - 3, cy + 9, cx + 1, cy + 9, BOLT);
      c.tri(cx - 1, cy + 8, cx + 3, cy + 8, cx - 2, cy + 14, BOLT);
    } else if (snow) {
      for (int i = -1; i <= 1; ++i) {
        c.smoothCircle(cx + i * 6, cy + 7 + (i & 1) * 3, 1, CLOUD);
      }
    } else {
      for (int i = -1; i <= 1; ++i) c.wideLine(cx + i * 5 + 1, cy + 5, cx + i * 5 - 1, cy + 10, 0.8f, RAIN);
    }
  }
}

}  // namespace icons
