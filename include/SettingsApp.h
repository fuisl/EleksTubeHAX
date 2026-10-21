#ifndef SETTINGS_APP_H
#define SETTINGS_APP_H

/*
 * The settings menu (MODE long hold), drawn on the lower half of the leftmost tube over the running clock.
 *   MODE tap: next item   LEFT/RIGHT tap: change value   POWER tap: leave
 * Leaves automatically after 10 s without input; the config is saved when leaving.
 */
#include "App.h"
#include "ClockApp.h"

class SettingsApp : public App
{
public:
  explicit SettingsApp(ClockApp &clock) : clock(clock) {}
  const char *name() const override { return "settings"; }
  void enter() override;
  void exit() override;
  void loop() override;
  bool onButton(const ButtonEvent &event) override;

private:
  enum Item
  {
    backlight_pattern,   // Change the backlight patterns.
    pattern_color,       // Change the backlight pattern color.
    backlight_intensity, // Change how bright the backlight LEDs are.
    twelve_hour,         // Select 12 hour or 24 hour format.
    blank_hours_zero,    // Whether to blank the leading zero in the hours column.
    utc_offset_hour,     // Change the UTC offset by an hour.
    utc_offset_15m,      // Change the UTC offset by 15 minutes.
    selected_graphic,    // Select clock "font" 0...9 -> first char in file name "00.bmp to 90.bmp".
#ifdef WIFI_USE_WPS
    start_wps, // Connect to WiFi using WPS pushbutton mode.
#endif
    num_items
  };
  static const uint32_t idle_timeout_ms = 10000;

  ClockApp &clock;
  Item item = backlight_pattern;
  uint32_t last_input_ms = 0;
  uint8_t shown_hour = 255;

  void apply(int8_t change); // Apply a LEFT (-1) / RIGHT (+1) change to the current item and redraw it
  void adjustUtcOffset(time_t step);
};

#endif // SETTINGS_APP_H
