#include "Input.h"

bool Input::gesture(Button &b, uint8_t idx, Gesture &out)
{
  if (b.isDownEdge())
    long_hold_fired[idx] = false;

  if (b.isUpEdge())
  {
    out = Gesture::Tap;
    return true;
  }
  if (b.isDownLongEdge())
  {
    out = Gesture::Hold;
    return true;
  }
  // millisInState() keeps counting from the press through the long-press edge, so it is the total hold time.
  if (b.isDownLong() && !long_hold_fired[idx] && b.millisInState() >= BUTTON_LONG_HOLD_MS)
  {
    long_hold_fired[idx] = true;
    out = Gesture::LongHold;
    return true;
  }
  return false;
}

uint8_t Input::poll(Buttons &buttons, ButtonEvent events[4])
{
  Button *all[4] = {&buttons.left, &buttons.mode, &buttons.right, &buttons.power};
  uint8_t count = 0;
  for (uint8_t i = 0; i < 4; i++)
  {
    Gesture g;
    if (gesture(*all[i], i, g))
      events[count++] = ButtonEvent{ButtonId(i), g};
  }
  return count;
}
