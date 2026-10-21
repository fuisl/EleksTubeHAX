#ifndef APP_H
#define APP_H

/*
 * Modes ("apps") of the clock and the manager that switches between them.
 *
 * Exactly one app is active and owns the displays. Every app gets background() on every loop,
 * so timers (e.g. a running countdown) keep going while another app is shown.
 *
 * Global button map (docs/DESIGN.md), applied when the active app does not consume the gesture:
 *   MODE   tap: next app   hold: home (clock)   long hold: settings
 *   POWER  tap: displays + backlights on/off    hold: backlights on/off
 */
#include <Arduino.h>
#include "Input.h"

class App
{
public:
  virtual ~App() {}
  virtual const char *name() const = 0;

  // Became active: draw everything (the previous app left arbitrary content on the displays).
  virtual void enter() = 0;
  // About to become inactive.
  virtual void exit() {}
  // Active: update the displays.
  virtual void loop() = 0;
  // Every loop, active or not.
  virtual void background() {}
  // Return true when the gesture was handled and the global button map must not act on it.
  virtual bool onButton(const ButtonEvent &event) { return false; }
};

class AppManager
{
public:
  // home: target of MODE hold and the app shown at start. settings: target of MODE long hold.
  // cycle: apps visited by MODE tap, in order.
  void begin(App *home, App *settings, App *const *cycle, uint8_t cycle_count);
  void loop(Buttons &buttons);
  // Simulated button gestures from the serial port (USB): l m r p = tap, L M R P = hold, S = MODE long hold.
  void pollSerial();

  void switchTo(App *app);
  void back(); // Return to the app that was active before the current one (used when leaving settings).
  void redraw(); // Full redraw of the active app, e.g. after a dimming change.

  App *active() { return current; }
  App *byName(const char *name); // Home, settings or a cycle app; nullptr if unknown
  bool isActive(const App *app) { return current == app; }

  void setDisplaysOn(bool on);
  bool displaysOn();

private:
  App *home = nullptr;
  App *settings = nullptr;
  App *previous = nullptr;
  App *current = nullptr;
  App *const *cycle = nullptr;
  uint8_t cycle_count = 0;
  Input input;

  void handleGlobal(const ButtonEvent &event);
  void dispatch(const ButtonEvent &event);
  void nextInCycle();
};

extern AppManager apps;

#endif // APP_H
