// Screen manager, touch handling and shared widgets
#pragma once
#include "display.h"
#include "theme.h"

namespace ui {

enum ScreenId : uint8_t {
  SCR_RADAR, SCR_SETTINGS, SCR_LOCATION, SCR_WIFI, SCR_WIFI_JOIN,
  SCR_RADAR_PREFS, SCR_DISPLAY_PREFS, SCR_ABOUT, SCR_SETUP, SCR_NIGHT, SCR_MILITARY, SCR_COUNT
};

struct Screen {
  virtual ~Screen() {}
  virtual void enter() {}
  virtual void leave() {}
  virtual void render(Canvas& c) = 0;     // called once per 80-px band
  virtual void onDown(int x, int y) {}
  virtual void onDrag(int x, int y) {}
  virtual void onTap(int x, int y) {}
  virtual void tick(uint32_t now) {}
  virtual bool animated() { return false; }  // redraw every frame
};

// Screen instances (defined in the screen_*.cpp files)
Screen* radarScreen();
Screen* settingsScreen();
Screen* locationScreen();
Screen* wifiScreen();
Screen* wifiJoinScreen();
Screen* radarPrefsScreen();
Screen* displayPrefsScreen();
Screen* aboutScreen();
Screen* setupScreen();
Screen* nightScreen();
Screen* militaryScreen();

void begin();
void loop();
void go(ScreenId id);
ScreenId current();
void invalidate();
void applyDisplaySettings();     // brightness / flip changed
bool isNightNow();               // inside the night-dimming window right now
void previewNight(uint32_t ms);  // show the night brightness for a moment
void formatClock(char* out, size_t cap, int minutes);  // "10:00 PM" or "22:00"

// Pass-through helpers used by the screens
bool hit(int x, int y, int rx, int ry, int rw, int rh);
bool pressedIn(int rx, int ry, int rw, int rh);   // finger is down inside
void openJoin(const char* ssid, bool hidden);      // Wi-Fi password screen
void openLocation(bool firstRun);

// --- Widgets (sizes in device pixels) ---
constexpr int HEADER_H = 36;
// Header with back chevron, title, optional right pill button (e.g. "Save")
void header(Canvas& c, const char* title, const char* pill = nullptr, bool backArrow = true);
bool headerBackHit(int x, int y);
bool headerPillHit(int x, int y);
// Settings row: icon box + label + sub label + chevron
typedef void (*IconFn)(Canvas&, int cx, int cy);
void navRow(Canvas& c, int y, int h, IconFn icon, const char* label, const char* sub, bool chevron = true);
void toggle(Canvas& c, int x, int y, bool on);                        // 30 x 17
void segmented(Canvas& c, int x, int y, int w, int h, const char* const* labels, int n, int sel,
               FontId f = F_UI9);
int segmentedHit(int tx, int ty, int x, int y, int w, int h, int n);  // -1 if none
void button(Canvas& c, int x, int y, int w, int h, const char* label, FontId f, uint16_t bg,
            uint16_t fg, int radius = 6);
void rowDivider(Canvas& c, int y);

}  // namespace ui
