#ifndef CLAWD_H
#define CLAWD_H

/*
 * The little orange block with eyes, arms and legs that lives on the leftmost tube in Claude mode.
 * Drawn on a 15 x 16 grid of 9 px cells; only cells that change are redrawn.
 *
 * Idle it blinks, looks around, waves, types on a tiny keyboard and hops now and then. It reacts to:
 *   poke()        a button press: hop and wave
 *   lookRight()   something changed on the tube to its right: look over and type
 *   mood          worried (an incident is open: sweat drop, glancing, arms up), sleepy (night: eyes shut, zzz)
 */
#include <Arduino.h>

class Clawd
{
public:
  enum Mood : uint8_t
  {
    normal,
    worried,
    sleepy
  };

  void update(uint32_t ms, Mood mood); // Advance the animation and draw (tube must be selected)
  void redraw() { force = true; }      // Next update draws every cell
  void poke();
  void lookRight();

private:
  static const uint8_t COLS = 15, ROWS = 16, CELL = 9;
  static const int16_t TOP = 36; // y of the grid on the tube

  enum Action : uint8_t
  {
    idle,
    blink,
    look_left,
    look_right,
    wave,
    type,
    hop,
    panic
  };
  Action action = idle;
  uint32_t action_start = 0, action_length = 0;
  uint32_t next_action = 0;
  bool force = true;
  uint8_t shown[ROWS][COLS] = {};

  void start(Action a, uint32_t ms, uint32_t length);
  void compose(uint8_t grid[ROWS][COLS], uint32_t ms, Mood mood);
};

#endif // CLAWD_H
