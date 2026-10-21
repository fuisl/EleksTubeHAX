// Desktop stand-ins for the Arduino API, just enough to compile the pixel-theme drawing code.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>
#include <type_traits>

using std::max;
using std::min;
template <class T, class L, class H> T constrain(T v, L lo, H hi) { return v < lo ? lo : v > hi ? hi : v; }
uint32_t millis();
long random(long n);
long random(long a, long b);
#define PROGMEM
#define NUM_DIGITS (6)
#define SECONDS_ONES (0)
#define SECONDS_TENS (1)
#define MINUTES_ONES (2)
#define MINUTES_TENS (3)
#define HOURS_ONES (4)
#define HOURS_TENS (5)
#define NUM_BACKLIGHT_LEDS (6)

class String
{
public:
  String() {}
  String(const char *s) : s_(s ? s : "") {}
  String(const std::string &s) : s_(s) {}
  String(char c) : s_(1, c) {}
  template <class T, class = std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>>>
  String(T v) : s_(std::is_same_v<T, bool> ? (v ? "1" : "0") : std::to_string((long long)v)) {}
  const char *c_str() const { return s_.c_str(); }
  size_t length() const { return s_.size(); }
  bool operator==(const String &o) const { return s_ == o.s_; }
  bool operator!=(const String &o) const { return s_ != o.s_; }
  String &operator+=(const String &o) { s_ += o.s_; return *this; }
  String &operator+=(char c) { s_ += c; return *this; }
  friend String operator+(String a, const String &b) { return a += b; }
  friend String operator+(String a, const char *b) { return a += String(b); }
  friend String operator+(String a, char c) { return a += c; }
  template <class T, class = std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>>>
  friend String operator+(String a, T v) { return a += String(v); }

private:
  std::string s_;
};
