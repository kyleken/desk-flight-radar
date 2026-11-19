// Military aircraft: highlight on/off and color, with a preview on the current theme
#include "icons.h"
#include "settings.h"
#include "ui.h"

namespace ui {
namespace {

constexpr int CELL_W = 53, CELL_H = 40, CELL_GAP_X = 4, CELL_GAP_Y = 5, GRID_X = 8, GRID_Y = 88;
constexpr int PV_X = 8, PV_Y = 182, PV_W = 224, PV_H = 112;

const RadarTheme& currentTheme() {
  switch (g_set.theme) {
    case T_AMBER: return THEME_AMBER;
    case T_HOLIDAY: return THEME_HOLIDAY;
    default: return THEME_CLASSIC;
  }
}

class MilitaryScreen : public Screen {
 public:
  void leave() override { settingsSave(); }

  void render(Canvas& c) override {
    c.fillRect(0, c.oy, 240, cfg::BAND_H, ui_c::SCREEN_BG);
    header(c, "Military aircraft");
    const bool on = g_set.milHighlight;

    if (c.visible(36, 34)) {
      c.fillRect(0, 36, 240, 34, ui_c::ROW_BG);
      rowDivider(c, 69);
      c.text("Highlight military aircraft", 8, 47, F_UI11, ui_c::TEXT, lgfx::middle_left);
      c.text("Planes the feeds flag as military", 8, 60, F_UI8, ui_c::TEXT_DIM, lgfx::middle_left);
      toggle(c, 202, 45, on);
    }

    if (c.visible(74, 100)) {
      c.text("COLOR", 8, 80, F_UI8, ui_c::TEXT_DIM, lgfx::middle_left);
      for (int i = 0; i < MIL_COLOR_COUNT; ++i) {
        int x, y;
        cell(i, x, y);
        const NamedColor& nc = MIL_COLORS[i];
        bool sel = i == g_set.milColor;
        c.roundRect(x, y, CELL_W, CELL_H, 6, pressedIn(x, y, CELL_W, CELL_H) && on ? ui_c::BTN_PRESSED : ui_c::ROW_BG);
        c.s->drawRoundRect(x, y - c.oy, CELL_W, CELL_H, 6, sel && on ? ui_c::CYAN : ui_c::KEY_BORDER);
        if (sel && on) c.s->drawRoundRect(x + 1, y + 1 - c.oy, CELL_W - 2, CELL_H - 2, 5, ui_c::CYAN);
        uint16_t col = rgb(nc.r, nc.g, nc.b);
        if (!on) col = ui_c::TEXT_FAINT;
        c.smoothCircle(x + CELL_W / 2, y + 15, 8, col);
        if (sel && on) icons::check(c, x + CELL_W / 2, y + 15, 8, ui_c::STRIP_BG);
        c.text(nc.name, x + CELL_W / 2, y + 32, F_UI8, on ? ui_c::TEXT : ui_c::TEXT_FAINT, lgfx::middle_center);
      }
    }

    // preview on the user's radar theme
    if (c.visible(PV_Y, PV_H + 26)) {
      const RadarTheme& T = currentTheme();
      c.fillRect(PV_X, PV_Y, PV_W, PV_H, T.bg);
      int cx = PV_X + PV_W / 2, cy = PV_Y + PV_H / 2;
      c.fillRect(cx - 1, PV_Y, 2, PV_H, T.cross);
      c.fillRect(PV_X, cy - 1, PV_W, 2, T.cross);
      c.ring(cx, cy, 46, 1.4f, T.ringDim);
      c.ring(cx, cy, 4, 1.4f, T.home);
      c.roundFrame(PV_X, PV_Y, PV_W, PV_H, 4, ui_c::BORDER);
      // a normal airliner and a military transport
      plane(c, PV_X + 46, PV_Y + 30, 110, T.plane, T.label, T.labelSub, "UAL1423", "B38M 12.4k", false);
      uint16_t mil = on ? militaryColor() : T.plane;
      plane(c, PV_X + 176, PV_Y + 80, 250, mil, on ? mil : T.label, T.labelSub, "RCH431", "C17 15.2k", true);
      char cap[48];
      static const char* names[] = {"Classic", "Amber", "Holiday"};
      if (on) snprintf(cap, sizeof(cap), "Preview on your %s theme", names[g_set.theme]);
      else strcpy(cap, "Off: military planes look like the rest");
      c.text(cap, 120, 308, F_UI8, ui_c::TEXT_FAINT, lgfx::middle_center);
    }
  }

  void onTap(int x, int y) override {
    if (headerBackHit(x, y)) return go(SCR_SETTINGS);
    if (hit(x, y, 0, 36, 240, 34)) {
      g_set.milHighlight = !g_set.milHighlight;
    } else if (g_set.milHighlight) {
      for (int i = 0; i < MIL_COLOR_COUNT; ++i) {
        int cx, cy;
        cell(i, cx, cy);
        if (hit(x, y, cx - 1, cy - 2, CELL_W + 3, CELL_H + 4)) g_set.milColor = i;
      }
    }
    invalidate();
  }

 private:
  static void cell(int i, int& x, int& y) {
    x = GRID_X + (i % 4) * (CELL_W + CELL_GAP_X);
    y = GRID_Y + (i / 4) * (CELL_H + CELL_GAP_Y);
  }

  // label on the side facing the preview's center, like the real radar
  static void plane(Canvas& c, int x, int y, float hdg, uint16_t col, uint16_t l1c, uint16_t l2c, const char* l1,
                    const char* l2, bool labelLeft) {
    icons::plane(c, x, y, hdg, 13, col);
    int lx = labelLeft ? x - 10 : x + 10;
    auto d = labelLeft ? lgfx::top_right : lgfx::top_left;
    c.text(l1, lx, y - 9, F_MONO8, l1c, d);
    c.text(l2, lx, y, F_MONO8, l2c, d);
  }
};

MilitaryScreen s_military;

}  // namespace

Screen* militaryScreen() { return &s_military; }

}  // namespace ui
