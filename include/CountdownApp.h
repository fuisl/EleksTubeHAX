#ifndef COUNTDOWN_APP_H
#define COUNTDOWN_APP_H

/*
 * "Oppenheimer" countdown, up to 24 hours.
 * Shows HH MM SS; in the last minute it switches to 00 SS cc (seconds and hundredths), with the two
 * hundredths tubes drawn from the FastDigits RAM cache as fast as the SPI bus allows.
 *
 * Buttons:  stopped  LEFT/RIGHT tap: -/+ time (1 min steps up to 10 min, 5 up to 1 h, 15 up to 3 h, then 1 h)
 *                    RIGHT hold: start          LEFT hold: clear to 00:00
 *           running  RIGHT hold: pause/resume   LEFT hold: reset to the set time
 *           done     any button: dismiss
 * It keeps running in the background; the backlights show the remaining time, and the clock
 * switches to the countdown when it reaches zero.
 *
 * The public control functions are used by Remote (MQTT, web API, iPhone shortcut); main loop only.
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

  void start(uint32_t duration_ms); // Set the duration and start right away
  void setDuration(uint32_t duration_ms); // Stops a running countdown
  void pause();
  void resume();
  void reset(); // Back to the set duration, stopped
  const char *stateName();
  uint32_t durationMs() { return set_ms; }
  uint32_t remaining();

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
  bool fast_active = false; // FastDigits cache is loaded (from shortly before the last minute)
  uint8_t fast_shown[2] = {255, 255};
  int8_t blink_shown = -1;
  uint16_t frames = 0; // Hundredths frames drawn since frames_since (logged while running)
  uint32_t frames_since = 0;
  FastDigits fast;

  void setState(State new_state);
  void adjustMinutes(int8_t direction);
  void drawAll(bool force);
  void drawDone();
  void updateBacklights();
  void releaseResources();
};

#endif // COUNTDOWN_APP_H
