#pragma once
#include <ctime>
#include <cstdint>
enum timeStatus_t { timeNotSet, timeNeedsSync, timeSet };
timeStatus_t timeStatus();
time_t now();
int weekday(time_t t); // 1 = Sunday
