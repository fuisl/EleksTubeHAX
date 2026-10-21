#ifndef COUNTDOWN_APP_H
#define COUNTDOWN_APP_H

/*
 * "Oppenheimer" countdown: MM SS cc (minutes, seconds, hundredths), up to 99:59.99.
 * Minutes and seconds are full-resolution nixie images; while running, the two hundredths tubes
 * are drawn from the FastDigits RAM cache as fast as the SPI bus allows.
 *
 * Buttons:  stopped  LEFT/RIGHT tap: -/+ time (1 min steps up to 10, 5 up to 60, then 10)
 *                    RIGHT hold: start          LEFT hold: clear to 00:00
 *           running  RIGHT hold: pause/resume   LEFT hold: reset to the set time
 *           done     any button: dismiss
 * It keeps running in the background; the backlights show the remaining time, and the clock
 * switches to the countdown when it reaches zero.
 */
#include "App.h"
#include "FastDigits.h"

class CountdownApp : public App
{
public:
  const char *name() const override { return "countdown"; }
  void enter() override;
  void exit() override;
  void loop() override;
  void background() override;
  bool onButton(const ButtonEvent &event) override;

private:
  enum State
  {
    stopped,
    running,
    paused,
    done
  };
  State state = stopped;
  uint32_t set_ms = 5UL * 60 * 1000; // Duration chosen with LEFT/RIGHT
  uint32_t remaining_ms = 5UL * 60 * 1000;
  uint32_t end_millis = 0;  // millis() at zero, while running
  uint32_t done_millis = 0; // millis() when zero was reached
  bool fast_active = false; // Hundredths drawn from the RAM cache
  uint8_t fast_shown[2] = {255, 255};
  int8_t blink_shown = -1;
  uint16_t frames = 0; // Hundredths frames drawn since frames_since (logged while running)
  uint32_t frames_since = 0;
  FastDigits fast;

  uint32_t remaining();
  void setState(State new_state);
  void adjustMinutes(int8_t direction);
  void drawAll(bool force);
  void drawDone();
  void updateBacklights();
  void releaseResources();
};

#endif // COUNTDOWN_APP_H
