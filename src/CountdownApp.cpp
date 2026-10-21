#include "CountdownApp.h"
#include "Backlights.h"
#include "TFTs.h"

static const uint32_t DONE_SHOW_MS = 10000;    // How long the "00 00 00" alarm runs before resetting
static const uint32_t LAST_MINUTE_MS = 60000;   // Below this: 00 SS cc instead of HH MM SS
static const uint32_t CACHE_AHEAD_MS = 65000;   // Build the hundredths cache (~0.35 s) while digits still change once a second
static const int32_t MAX_MINUTES = 24 * 60;
static const uint32_t FINAL_SECONDS_MS = 10000; // Backlight tension phase before zero

static uint32_t scaleColor(uint32_t color, float f)
{
  uint8_t r = ((color >> 16) & 0xFF) * f, g = ((color >> 8) & 0xFF) * f, b = (color & 0xFF) * f;
  return (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
}

uint32_t CountdownApp::remaining()
{
  if (state == running)
  {
    int32_t left = int32_t(end_millis - millis());
    return left > 0 ? left : 0;
  }
  return state == done ? 0 : remaining_ms;
}

void CountdownApp::setState(State new_state)
{
  if (new_state == running)
    end_millis = millis() + remaining_ms;
  else if (new_state == paused)
    remaining_ms = remaining();
  else if (new_state == stopped)
    remaining_ms = set_ms;
  else if (new_state == done)
  {
    done_millis = millis();
    remaining_ms = 0;
  }
  state = new_state;
  Serial.printf("Countdown: %s, %lu ms left\n", stateName(), remaining());
  blink_shown = -1;
  if (state != running)
    releaseResources(); // Hundredths go back to full-resolution images
  if (apps.isActive(this) && apps.displaysOn() && state != done && state != running)
    drawAll(true);
}

const char *CountdownApp::stateName()
{
  return state == running ? "running" : state == paused ? "paused" : state == stopped ? "stopped" : "done";
}

void CountdownApp::setDuration(uint32_t duration_ms)
{
  set_ms = min(duration_ms, uint32_t(MAX_MINUTES) * 60000);
  setState(stopped);
}

void CountdownApp::start(uint32_t duration_ms)
{
  setDuration(duration_ms);
  if (set_ms == 0)
    return;
  setState(running);
  apps.switchTo(this);
}

void CountdownApp::pause()
{
  if (state == running)
    setState(paused);
}

void CountdownApp::resume()
{
  if (state == paused || (state == stopped && remaining_ms > 0))
    setState(running);
}

void CountdownApp::reset()
{
  setState(stopped);
}

void CountdownApp::adjustMinutes(int8_t direction)
{
  // Steps of 1 min up to 10 min, 5 up to 1 h, 15 up to 3 h, then 1 h: any value up to an hour in ~20 taps.
  int32_t m = set_ms / 60000;
  if (direction > 0)
    m = m < 10 ? m + 1 : m < 60 ? (m / 5 + 1) * 5 : m < 180 ? (m / 15 + 1) * 15 : min((m / 60 + 1) * 60, MAX_MINUTES);
  else
    m = m > 180 ? ((m - 1) / 60) * 60 : m > 60 ? ((m - 1) / 15) * 15 : m > 10 ? ((m - 1) / 5) * 5 : max(m - 1, int32_t(0));
  set_ms = uint32_t(m) * 60000;
  remaining_ms = set_ms;
  drawAll(false);
}

void CountdownApp::enter()
{
  tfts.show_status_overlays = false;
  tfts.preload_step = -1; // Counting down; preload_position is set by drawAll()
  fast_active = false;
  blink_shown = -1;
  if (state == done)
    drawDone();
  else
    drawAll(true);
}

void CountdownApp::exit()
{
  releaseResources();
}

void CountdownApp::releaseResources()
{
  fast_active = false;
  fast.release(); // ~87 KB back to the heap; rebuilt in ~0.4 s when needed again
}

void CountdownApp::loop()
{
  if (state == done)
  {
    drawDone();
    return;
  }

  bool want_fast = state == running && remaining() < CACHE_AHEAD_MS;
  if (want_fast && !fast_active)
  {
    fast_active = fast.ensure(tfts.current_graphic, tfts.dimming);
    fast_shown[0] = fast_shown[1] = 255;
    frames = 0;
    frames_since = millis();
  }
  else if (!want_fast && fast_active)
  {
    releaseResources();
    drawAll(true); // Back to full-resolution hundredths
    return;
  }
  drawAll(false);
}

void CountdownApp::drawAll(bool force)
{
  uint32_t t = remaining();
  bool last_minute = t < LAST_MINUTE_MS;
  uint8_t left, middle, right; // Pairs of tubes: HH MM SS, or 00 SS cc in the last minute
  if (last_minute)
  {
    left = 0;
    middle = t / 1000;
    right = (t / 10) % 100;
  }
  else
  {
    uint32_t s = (t + 999) / 1000; // Round up like a kitchen timer: 05:00 shows for the first second
    left = s / 3600;
    middle = (s / 60) % 60;
    right = s % 60;
  }
  // Pre-load the next image of the tube that changes once per second.
  tfts.preload_position = last_minute ? MINUTES_ONES : SECONDS_ONES;
  TFTs::show_t show = force ? TFTs::force : TFTs::yes;

  if (fast_active && last_minute && state == running)
  {
    // Hundredths first: they are the animation; nixie images below only change once per second.
    uint8_t values[2] = {uint8_t(right % 10), uint8_t(right / 10)}; // SECONDS_ONES, SECONDS_TENS
    for (uint8_t i = 0; i < 2; i++)
      if (force || values[i] != fast_shown[i])
      {
        fast.draw(i == 0 ? SECONDS_ONES : SECONDS_TENS, values[i]);
        fast_shown[i] = values[i];
        frames += i == 0;
      }
    if (millis() - frames_since >= 10000)
    {
      Serial.printf("Countdown: hundredths at %u fps, heap free %u min %u largest %u\n", frames / 10,
                    ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
      frames = 0;
      frames_since = millis();
    }
  }
  else
  {
    if (fast_shown[0] != 255)
    { // Leaving the fast tubes: their full-resolution images must be redrawn
      show = TFTs::force;
      fast_shown[0] = fast_shown[1] = 255;
    }
    tfts.setDigit(SECONDS_ONES, right % 10, show);
    tfts.setDigit(SECONDS_TENS, right / 10, show);
  }
  tfts.setDigit(MINUTES_ONES, middle % 10, show);
  tfts.setDigit(MINUTES_TENS, middle / 10, show);
  tfts.setDigit(HOURS_ONES, left % 10, show);
  tfts.setDigit(HOURS_TENS, left / 10, show);
}

void CountdownApp::drawDone()
{
  int8_t on = ((millis() - done_millis) / 400) % 2 == 0;
  if (on == blink_shown)
    return;
  blink_shown = on;
  for (uint8_t pos = 0; pos < NUM_DIGITS; pos++)
    tfts.setDigit(pos, on ? 0 : TFTs::blanked, TFTs::force);
}

void CountdownApp::background()
{
  if (state == running && remaining() == 0)
  {
    setState(done);
    apps.switchTo(this); // Pop up wherever the clock is
  }
  else if (state == done && millis() - done_millis > DONE_SHOW_MS)
  {
    setState(stopped);
  }
  updateBacklights();
}

void CountdownApp::updateBacklights()
{
  if (state == stopped)
  {
    backlights.releaseOverride();
    return;
  }

  uint32_t now = millis();
  uint32_t color = backlights.getColor(); // The clock's own (calibrated) backlight color
  if (state == done)
  {
    // Calm alarm: steady clock color; the blinking 00 on the tubes is the signal.
    for (uint8_t led = 0; led < NUM_BACKLIGHT_LEDS; led++)
      backlights.setOverride(led, color);
    return;
  }

  uint32_t t = remaining();
  if (state == running && t <= FINAL_SECONDS_MS)
  {
    // Last seconds: all LEDs breathe softly in the clock color, one cycle per second, brightest on each second change.
    float pulse = 0.65f + 0.35f * cosf(2.0f * M_PI * (t % 1000) / 1000.0f);
    uint32_t c = scaleColor(color, pulse);
    for (uint8_t led = 0; led < NUM_BACKLIGHT_LEDS; led++)
      backlights.setOverride(led, c);
    return;
  }

  // Progress bar draining from right to left; the last lit LED fades. Paused: slow breathing.
  float lit = set_ms > 0 ? NUM_BACKLIGHT_LEDS * float(t) / set_ms : 0;
  float breath = state == paused ? 0.3f + 0.7f * fabsf(sinf(M_PI * now / 2000.0f)) : 1.0f;
  for (uint8_t p = 0; p < NUM_BACKLIGHT_LEDS; p++) // p = 0 is the leftmost tube (HOURS_TENS)
  {
    float level = constrain(lit - p, 0.0f, 1.0f);
    backlights.setOverride(HOURS_TENS - p, scaleColor(color, level * breath));
  }
}

bool CountdownApp::onButton(const ButtonEvent &event)
{
  if (state == done)
  {
    setState(stopped); // Any button dismisses the alarm
    return true;
  }

  bool left = event.button == ButtonId::Left, right = event.button == ButtonId::Right;
  if (!left && !right)
    return false; // MODE / POWER keep their global meaning

  if (state == stopped)
  {
    if (event.gesture == Gesture::Tap)
      adjustMinutes(left ? -1 : 1);
    else if (event.gesture == Gesture::Hold && right && remaining_ms > 0)
      setState(running);
    else if (event.gesture == Gesture::Hold && left)
    {
      set_ms = 0;
      setState(stopped);
    }
  }
  else if (event.gesture == Gesture::Hold) // running or paused
  {
    if (right)
      setState(state == running ? paused : running);
    else
      setState(stopped);
  }
  return true;
}
