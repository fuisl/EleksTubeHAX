#ifndef INPUT_H
#define INPUT_H

/*
 * Turns the raw button states into gestures the modes react to:
 *   Tap       released before the hold threshold
 *   Hold      held for BUTTON_HOLD_MS (fires once, while still held)
 *   LongHold  held for BUTTON_LONG_HOLD_MS (fires once, while still held, after Hold)
 */
#include <Arduino.h>
#include "Buttons.h"

enum class ButtonId : uint8_t
{
  Left,
  Mode,
  Right,
  Power
};

enum class Gesture : uint8_t
{
  Tap,
  Hold,
  LongHold
};

struct ButtonEvent
{
  ButtonId button;
  Gesture gesture;
};

class Input
{
public:
  // Call once per loop, after buttons.loop(). Returns the number of events written (max 4, one per button).
  uint8_t poll(Buttons &buttons, ButtonEvent events[4]);

private:
  bool long_hold_fired[4] = {false, false, false, false};
  bool gesture(Button &b, uint8_t idx, Gesture &out);
};

#endif // INPUT_H
