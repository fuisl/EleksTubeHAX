#include "SettingsApp.h"
#include "Backlights.h"
#include "Clock.h"
#include "StoredConfig.h"
#include "WiFi_WPS.h"

extern StoredConfig stored_config; // main.cpp
#ifdef DIMMING
void checkDimmingNeeded(); // main.cpp
#endif

static void prepareMenuArea()
{
  tfts.chip_select.setHoursTens();
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);
  tfts.fillRect(0, 120, 135, 120, TFT_BLACK); // use lower half of the display, fill with black
  tfts.setCursor(0, 124, 4);                  // use font 4 - 26 pixel high - for the menu text
}

void SettingsApp::enter()
{
  item = backlight_pattern;
  last_input_ms = millis();
  clock.enter(); // The menu is drawn over the running clock
  shown_hour = uclock.getHour24();
  apply(0);
}

void SettingsApp::exit()
{
  Serial.print("Saving config! Triggered from leaving menu...");
  stored_config.save();
  Serial.println(" Done.");
}

void SettingsApp::loop()
{
  if (millis() - last_input_ms > idle_timeout_ms)
  {
    apps.back();
    return;
  }
  clock.drawTime(TFTs::yes);
  if (uclock.getHour24() != shown_hour)
  { // The hours digit was redrawn over the menu text
    shown_hour = uclock.getHour24();
    apply(0);
  }
}

bool SettingsApp::onButton(const ButtonEvent &event)
{
  last_input_ms = millis();
  if (event.gesture != Gesture::Tap)
    return false; // MODE hold (home) and POWER hold keep their global meaning

  switch (event.button)
  {
  case ButtonId::Mode:
    item = Item((item + 1) % num_items);
    apply(0);
    break;
  case ButtonId::Left:
    apply(-1);
    break;
  case ButtonId::Right:
    apply(1);
    break;
  case ButtonId::Power:
    apps.back();
    break;
  }
  return true;
}

void SettingsApp::adjustUtcOffset(time_t step)
{
  // Allowed range is -12 to +12 hours. Passing one end wraps to the other; with a whole-hour offset this keeps
  // the shown time (+12 <-> -12), otherwise the minutes part changes (i.e. from 11:45 directly to -11:15).
  time_t newOffset = uclock.getTimeZoneOffset() + step;
  bool offsetWrapAround = false;
  if (newOffset > 43200)
  { // we just "passed" +12 hours -> set to -12 hours
    newOffset = -43200;
    offsetWrapAround = true;
  }
  if (newOffset < -43200 && !offsetWrapAround)
  { // we just passed -12 hours -> set to +12 hours
    newOffset = 43200;
  }
  uclock.setTimeZoneOffset(newOffset);
  uclock.loop();
  clock.drawTime(TFTs::yes); // Redraws changed digits, including the hours tens under the menu text
#ifdef DIMMING
  checkDimmingNeeded(); // The hour may have moved into or out of night time
#endif
}

static void printUtcOffset(time_t offset)
{
  char offsetStr[11];
  int8_t offset_hour = offset / 3600;
  int8_t offset_min = (offset % 3600) / 60;
  if (offset_min == 0 && offset_hour == 0)
    snprintf(offsetStr, sizeof(offsetStr), "%d:%02d", 0, 0); // no sign for 0:00
  else if (offset_min <= 0 && offset_hour <= 0)
    snprintf(offsetStr, sizeof(offsetStr), "-%d:%02d", -offset_hour, -offset_min);
  else
    snprintf(offsetStr, sizeof(offsetStr), "+%d:%02d", offset_hour, offset_min);
  tfts.println(offsetStr);
}

void SettingsApp::apply(int8_t change)
{
  switch (item)
  {
  case backlight_pattern:
    if (change != 0)
      backlights.setNextPattern(change);
    prepareMenuArea();
    tfts.println("Pattern:");
    tfts.println(backlights.getPatternStr());
    break;

  case pattern_color:
    if (change != 0)
      backlights.adjustColorPhase(change * 16);
    prepareMenuArea();
    tfts.println("Color:");
    tfts.printf("%06X\n", backlights.getColor());
    break;

  case backlight_intensity:
    if (change != 0)
      backlights.adjustIntensity(change);
    prepareMenuArea();
    tfts.println("Intensity:");
    tfts.println(backlights.getIntensity());
    break;

  case twelve_hour:
    if (change != 0)
    {
      uclock.toggleTwelveHour();
      tfts.setDigit(HOURS_TENS, uclock.getHoursTens(), TFTs::force);
      tfts.setDigit(HOURS_ONES, uclock.getHoursOnes(), TFTs::force);
    }
    prepareMenuArea();
    tfts.println("Hour format");
    tfts.println(uclock.getTwelveHour() ? "12 hour" : "24 hour");
    break;

  case blank_hours_zero:
    if (change != 0)
    {
      uclock.toggleBlankHoursZero();
      tfts.setDigit(HOURS_TENS, uclock.getHoursTens(), TFTs::force);
    }
    prepareMenuArea();
    tfts.println("Blank zero?");
    tfts.println(uclock.getBlankHoursZero() ? "yes" : "no");
    break;

  case utc_offset_hour:
  case utc_offset_15m:
    if (change != 0)
      adjustUtcOffset(change * (item == utc_offset_hour ? 3600 : 900));
    prepareMenuArea();
    tfts.println("UTC Offset");
    tfts.println(item == utc_offset_hour ? " +/- Hour" : " +/- 15m");
    printUtcOffset(uclock.getTimeZoneOffset());
    break;

  case selected_graphic:
    if (change != 0)
    {
      uclock.adjustClockGraphicsIdx(change);
      if (tfts.current_graphic != uclock.getActiveGraphicIdx())
      {
        tfts.current_graphic = uclock.getActiveGraphicIdx();
        clock.drawTime(TFTs::force);
      }
    }
    prepareMenuArea();
    tfts.println("Selected");
    tfts.println("graphic:");
    tfts.printf("    %d\n", uclock.getActiveGraphicIdx());
    break;

#ifdef WIFI_USE_WPS
  case start_wps:
    if (change < 0)
    { // left button
      Serial.println("WiFi WPS start request");
      tfts.clear();
      tfts.fillScreen(TFT_BLACK);
      tfts.setTextColor(TFT_WHITE, TFT_BLACK);
      tfts.setCursor(0, 0, 4); // Font 4. 26 pixel high
      WiFiStartWps();
      clock.drawTime(TFTs::force);
    }
    prepareMenuArea();
    tfts.println("Connect to WiFi?");
    tfts.println("Left=WPS");
    break;
#endif

  default:
    break;
  }
}
