// Night dimming: schedule, night brightness (or screen off), tap to wake
#include "icons.h"
#include "net.h"
#include "settings.h"
#include "ui.h"

namespace ui {
namespace {

constexpr int SL_X = 16, SL_W = 208, SL_Y = 182;
constexpr int STEP_MIN = 30;

class NightScreen : public Screen {
 public:
  void leave() override {
    settingsSave();
    applyDisplaySettings();
  }

  void tick(uint32_t now) override {
    if (now - last_ > 1000) {  // "right now" line follows the clock
      last_ = now;
      invalidate();
    }
  }

  void render(Canvas& c) override {
    c.fillRect(0, c.oy, 240, cfg::BAND_H, ui_c::SCREEN_BG);
    header(c, "Night dimming");
    const bool on = g_set.nightDim;
    const uint16_t txt = on ? ui_c::TEXT : ui_c::TEXT_FAINT;
    const uint16_t dim = on ? ui_c::TEXT_DIM : ui_c::TEXT_FAINT;
    char buf[40];

    // on / off
    if (c.visible(36, 34)) {
      row(c, 36, 34);
      c.text("Dim at night", 8, 47, F_UI11, ui_c::TEXT, lgfx::middle_left);
      c.text("Lower the brightness on a schedule", 8, 60, F_UI8, ui_c::TEXT_DIM, lgfx::middle_left);
      toggle(c, 202, 45, on);
    }
    // start / end times
    stepperRow(c, 70, "Starts", g_set.nightStart, txt, on);
    stepperRow(c, 108, "Ends", g_set.nightEnd, txt, on);

    // night brightness slider
    if (c.visible(146, 50)) {
      row(c, 146, 50);
      c.text("Night brightness", 8, 160, F_UI11, txt, lgfx::middle_left);
      if (g_set.nightBright == 0) strcpy(buf, "Screen off");
      else snprintf(buf, sizeof(buf), "%d%%", g_set.nightBright);
      c.text(buf, 232, 160, F_MONO8, on ? ui_c::HUD_TEXT : ui_c::TEXT_FAINT, lgfx::middle_right);
      int fill = g_set.nightBright * SL_W / 60;
      c.roundRect(SL_X, SL_Y - 2, SL_W, 5, 2, ui_c::TOGGLE_OFF);
      c.roundRect(SL_X, SL_Y - 2, std::max(fill, 5), 5, 2, on ? ui_c::CYAN : ui_c::TEXT_FAINT);
      c.smoothCircle(SL_X + fill, SL_Y, 7, on ? rgb(255, 255, 255) : ui_c::TEXT_DIM);
    }

    // tap to wake
    if (c.visible(196, 34)) {
      row(c, 196, 34);
      c.text("Tap to wake", 8, 207, F_UI11, txt, lgfx::middle_left);
      c.text("Normal brightness for 30 s when touched", 8, 220, F_UI8, dim, lgfx::middle_left);
      toggle(c, 202, 205, g_set.tapWake);
    }

    // status + preview
    if (c.visible(236, 84)) {
      struct tm t;
      const char* st;
      uint16_t col = ui_c::TEXT_DIM;
      if (!on) st = "Night dimming is off";
      else if (!net::localTime(t)) st = "Waiting for the clock to sync...";
      else if (isNightNow()) { st = "Right now: night, screen dimmed"; col = ui_c::CYAN; }
      else st = "Right now: daytime, normal brightness";
      c.text(st, 120, 245, F_UI9, col, lgfx::middle_center);
      button(c, 40, 260, 160, 30, "Preview for 3 seconds", F_UI9, on ? ui_c::KEY_BG : ui_c::ROW_BG,
             on ? ui_c::TEXT : ui_c::TEXT_FAINT, 15);
      c.text("Times use your location's time zone", 120, 305, F_UI8, ui_c::TEXT_FAINT, lgfx::middle_center);
    }
  }

  void onDown(int x, int y) override { slide(x, y); }
  void onDrag(int x, int y) override { slide(x, y); }

  void onTap(int x, int y) override {
    if (headerBackHit(x, y)) return go(SCR_DISPLAY_PREFS);
    if (hit(x, y, 0, 36, 240, 34)) {
      g_set.nightDim = !g_set.nightDim;
      applyDisplaySettings();
      return invalidate();
    }
    if (!g_set.nightDim) return;
    if (hit(x, y, 110, 70, 40, 38)) step(g_set.nightStart, -STEP_MIN);
    else if (hit(x, y, 200, 70, 40, 38)) step(g_set.nightStart, STEP_MIN);
    else if (hit(x, y, 110, 108, 40, 38)) step(g_set.nightEnd, -STEP_MIN);
    else if (hit(x, y, 200, 108, 40, 38)) step(g_set.nightEnd, STEP_MIN);
    else if (hit(x, y, 0, 196, 240, 34)) g_set.tapWake = !g_set.tapWake;
    else if (hit(x, y, 36, 256, 168, 38)) previewNight(3000);
    applyDisplaySettings();
    invalidate();
  }

 private:
  static void row(Canvas& c, int y, int h) {
    c.fillRect(0, y, 240, h, ui_c::ROW_BG);
    rowDivider(c, y + h - 1);
  }

  void stepperRow(Canvas& c, int y, const char* label, uint16_t minutes, uint16_t txt, bool on) {
    if (!c.visible(y, 38)) return;
    row(c, y, 38);
    c.text(label, 8, y + 19, F_UI11, txt, lgfx::middle_left);
    uint16_t bg = on ? ui_c::KEY_BG : ui_c::ROW_BG;
    uint16_t fg = on ? ui_c::TEXT : ui_c::TEXT_FAINT;
    button(c, 116, y + 7, 26, 24, "", F_UI11, bg, fg, 5);
    icons::minus(c, 129, y + 19, 10, fg);
    char buf[16];
    formatClock(buf, sizeof(buf), minutes);
    c.text(buf, 175, y + 19, F_MONO11, fg, lgfx::middle_center);
    button(c, 208, y + 7, 26, 24, "", F_UI11, bg, fg, 5);
    icons::plus(c, 221, y + 19, 10, fg);
  }

  static void step(uint16_t& minutes, int delta) {
    int m = ((int)minutes + delta + 1440) % 1440;
    minutes = (uint16_t)(m / STEP_MIN * STEP_MIN);
  }

  void slide(int x, int y) {
    if (!g_set.nightDim || !hit(x, y, SL_X - 14, 166, SL_W + 28, 30)) return;
    int v = (x - SL_X) * 60 / SL_W;
    v = constrain(v, 0, 60);
    v = (v + 2) / 5 * 5;
    if (v != g_set.nightBright) {
      g_set.nightBright = v;
      invalidate();
    }
    if (v > 0) previewNight(1500);  // show what it looks like (not at "off", so the slider stays visible)
  }

  uint32_t last_ = 0;
};

NightScreen s_night;

}  // namespace

Screen* nightScreen() { return &s_night; }

}  // namespace ui
