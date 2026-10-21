#ifndef CLOCK_APP_H
#define CLOCK_APP_H

#include "App.h"
#include "TFTs.h"

// The time of day in nixie digits. LEFT/RIGHT tap: previous/next clock face.
class ClockApp : public App
{
public:
  const char *name() const override { return "clock"; }
  void enter() override;
  void loop() override;
  bool onButton(const ButtonEvent &event) override;

  // Draws HH MM SS; only changed digits unless show == TFTs::force. Also used behind the settings menu.
  void drawTime(TFTs::show_t show);
};

#endif // CLOCK_APP_H
