#include "ClockApp.h"
#include "Clock.h"

void ClockApp::enter()
{
  tfts.show_status_overlays = true;
  tfts.preload_position = SECONDS_ONES;
  tfts.preload_step = 1;
  drawTime(TFTs::force);
}

void ClockApp::loop()
{
  drawTime(TFTs::yes);
}

bool ClockApp::onButton(const ButtonEvent &event)
{
  if (event.gesture != Gesture::Tap || (event.button != ButtonId::Left && event.button != ButtonId::Right))
    return false;

  uclock.adjustClockGraphicsIdx(event.button == ButtonId::Left ? -1 : 1);
  if (tfts.current_graphic != uclock.getActiveGraphicIdx())
  {
    tfts.current_graphic = uclock.getActiveGraphicIdx();
    drawTime(TFTs::force);
  }
  return true;
}

void ClockApp::drawTime(TFTs::show_t show)
{
  // Seconds first: they change most often and their next image is pre-loaded.
  tfts.setDigit(SECONDS_ONES, uclock.getSecondsOnes(), show);
  tfts.setDigit(SECONDS_TENS, uclock.getSecondsTens(), show);
  tfts.setDigit(MINUTES_ONES, uclock.getMinutesOnes(), show);
  tfts.setDigit(MINUTES_TENS, uclock.getMinutesTens(), show);
  tfts.setDigit(HOURS_ONES, uclock.getHoursOnes(), show);
  tfts.setDigit(HOURS_TENS, uclock.getHoursTens(), show);
}
