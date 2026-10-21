#include "App.h"
#include "Backlights.h"
#include "TFTs.h"

void AppManager::begin(App *home_, App *settings_, App *const *cycle_, uint8_t cycle_count_)
{
  home = home_;
  settings = settings_;
  cycle = cycle_;
  cycle_count = cycle_count_;
  current = nullptr;
  switchTo(home);
}

void AppManager::switchTo(App *app)
{
  if (app == nullptr || app == current)
    return;
  if (current != nullptr)
  {
    current->exit();
    previous = current;
  }
  current = app;
  Serial.printf("Mode: %s\n", current->name());
  if (displaysOn())
    current->enter();
}

void AppManager::back()
{
  switchTo(previous != nullptr && previous != current ? previous : home);
}

void AppManager::redraw()
{
  if (current != nullptr && displaysOn())
    current->enter();
}

bool AppManager::displaysOn()
{
  return tfts.isEnabled();
}

void AppManager::setDisplaysOn(bool on)
{
  if (on == displaysOn())
    return;
  if (on)
  {
    tfts.enableAllDisplays();
    backlights.PowerOn();
    redraw(); // The displays were blanked before turning off
  }
  else
  {
    tfts.chip_select.setAll();
    tfts.fillScreen(TFT_BLACK); // Blank first; the panels keep their content otherwise
    tfts.disableAllDisplays();
    backlights.PowerOff();
  }
}

void AppManager::nextInCycle()
{
  uint8_t i = 0;
  while (i < cycle_count && cycle[i] != current)
    i++;
  // Not in the cycle (e.g. settings): start over at the first entry.
  switchTo(cycle[i < cycle_count ? (i + 1) % cycle_count : 0]);
}

void AppManager::handleGlobal(const ButtonEvent &event)
{
  if (event.button == ButtonId::Mode)
  {
    if (event.gesture == Gesture::Tap)
      nextInCycle();
    else if (event.gesture == Gesture::Hold)
      switchTo(home);
    else if (event.gesture == Gesture::LongHold)
      switchTo(settings);
  }
  else if (event.button == ButtonId::Power)
  {
    if (event.gesture == Gesture::Tap)
      setDisplaysOn(!displaysOn());
    else if (event.gesture == Gesture::Hold)
      backlights.togglePower();
  }
}

void AppManager::dispatch(const ButtonEvent &event)
{
  if (!displaysOn())
  {
    // While dark, only POWER tap does something: wake up.
    if (event.button == ButtonId::Power && event.gesture == Gesture::Tap)
      setDisplaysOn(true);
    return;
  }
  if (!current->onButton(event))
    handleGlobal(event);
}

void AppManager::pollSerial()
{
  while (Serial.available())
  {
    char c = Serial.read();
    const char *taps = "lmrp", *holds = "LMRP";
    const char *t = strchr(taps, c), *h = strchr(holds, c);
    if (c != '\0' && t != nullptr)
      dispatch(ButtonEvent{ButtonId(t - taps), Gesture::Tap});
    else if (c != '\0' && h != nullptr)
      dispatch(ButtonEvent{ButtonId(h - holds), Gesture::Hold});
    else if (c == 'S')
      dispatch(ButtonEvent{ButtonId::Mode, Gesture::LongHold});
  }
}

void AppManager::loop(Buttons &buttons)
{
  ButtonEvent events[4];
  uint8_t count = input.poll(buttons, events);
  for (uint8_t i = 0; i < count; i++)
    dispatch(events[i]);
  pollSerial();

  for (uint8_t i = 0; i < cycle_count; i++)
    cycle[i]->background();
  if (settings != nullptr)
    settings->background();

  if (displaysOn())
    current->loop();
}
