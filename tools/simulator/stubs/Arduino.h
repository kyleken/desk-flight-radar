// Host stub of the Arduino API, just enough for the UI code
#pragma once
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cctype>
#include <algorithm>
#include <string>
#include <strings.h>
extern uint32_t g_simMillis;
inline uint32_t millis() { return g_simMillis; }
inline void delay(uint32_t) {}
inline long random(long a, long b) { return a + rand() % (b - a); }
template <class T, class L, class H> T constrain(T x, L l, H h) { return x < l ? (T)l : (x > h ? (T)h : x); }
#define PI 3.1415926535897932384626433832795
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define HIGH 1
#define LOW 0
#define INPUT 1
#define OUTPUT 3
#define INPUT_PULLUP 5
#define F(x) x
#define FPSTR(x) x
#define PROGMEM
inline void pinMode(int, int) {}
inline int digitalRead(int) { return 1; }
inline void digitalWrite(int, int) {}
struct SerialStub {
  template <class... A> void printf(const char* f, A... a) { ::printf(f, a...); }
  void println(const char* s = "") { puts(s); }
  void begin(int) {}
};
extern SerialStub Serial;
struct EspStub { uint32_t getFreeHeap() { return 141 * 1024; } void restart() { exit(0); } };
extern EspStub ESP;
class String : public std::string {
 public:
  using std::string::string;
  String(const std::string& s) : std::string(s) {}
};
