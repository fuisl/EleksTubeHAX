#pragma once
#include <Arduino.h>
#include <TimeLib.h>
struct Clock
{
  time_t loop_time = 0;
  time_t getTimeZoneOffset() { return 7 * 3600; }
  uint8_t getHour();
  uint8_t getMinute();
  uint8_t getSecond();
  uint8_t getDay();
};
extern Clock uclock;
