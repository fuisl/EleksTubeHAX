#include "CountdownApp.h"
#include "Backlights.h"
#include "TFTs.h"

static const uint32_t DONE_SHOW_MS = 10000;    // How long the "00 00 00" alarm runs before resetting
static const uint32_t FINAL_SECONDS_MS = 10000; // Backlight tension phase before zero
static const uint32_t COLOR_ORANGE = 0xFF6E00;  // Same as the stock backlight color
static const uint32_t COLOR_RED = 0xFF0000;

static uint32_t scaleColor(uint32_t color, float f)
{
  uint8_t r = ((color >> 16) & 0xFF) * f, g = ((color >> 8) & 0xFF) * f, b = (color & 0xFF) * f;
  return (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
}

static uint32_t mixColor(uint32_t a, uint32_t b, float t)
{
  return scaleColor(a, 1.0f - t) + scaleColor(b, t); // Channels can't overflow: weights sum to 1
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
  Serial.printf("Countdown: %s, %lu ms left\n",
                state == running ? "running" : state == paused ? "paused" : state == stopped ? "stopped" : "done",
                remaining());
  blink_shown = -1;
  if (state != running)
    releaseResources(); // Hundredths go back to full-resolution images
  if (apps.isActive(this) && apps.displaysOn() && state != done && state != running)
    drawAll(true);
}

void CountdownApp::adjustMinutes(int8_t direction)
{
  // 1 minute steps up to 10, 5 up to 60, then 10 (max 99): any value up to an hour in ~20 taps.
  int16_t m = set_ms / 60000;
  if (direction > 0)
    m = m < 10 ? m + 1 : m < 60 ? (m / 5 + 1) * 5 : min((m / 10 + 1) * 10, 99);
  else
    m = m > 60 ? ((m - 1) / 10) * 10 : m > 10 ? ((m - 1) / 5) * 5 : max(m - 1, 0);
  set_ms = uint32_t(m) * 60000;
  remaining_ms = set_ms;
  drawAll(false);
}

void CountdownApp::enter()
{
  tfts.show_status_overlays = false;
  tfts.preload_position = MINUTES_ONES; // Seconds ones, counting down
  tfts.preload_step = -1;
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

  bool want_fast = state == running;
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
  uint8_t minutes = t / 60000, seconds = (t / 1000) % 60, hundredths = (t / 10) % 100;
  TFTs::show_t show = force ? TFTs::force : TFTs::yes;

  if (fast_active)
  {
    // Hundredths first: they are the animation; nixie images below only change once per second.
    uint8_t values[2] = {uint8_t(hundredths % 10), uint8_t(hundredths / 10)}; // SECONDS_ONES, SECONDS_TENS
    for (uint8_t i = 0; i < 2; i++)
      if (force || values[i] != fast_shown[i])
      {
        fast.draw(i == 0 ? SECONDS_ONES : SECONDS_TENS, values[i]);
        fast_shown[i] = values[i];
        frames += i == 0;
      }
    if (millis() - frames_since >= 10000)
    {
      Serial.printf("Countdown: hundredths at %u fps\n", frames / 10);
      frames = 0;
      frames_since = millis();
    }
  }
  else
  {
    tfts.setDigit(SECONDS_ONES, hundredths % 10, show);
    tfts.setDigit(SECONDS_TENS, hundredths / 10, show);
  }
  tfts.setDigit(MINUTES_ONES, seconds % 10, show);
  tfts.setDigit(MINUTES_TENS, seconds / 10, show);
  tfts.setDigit(HOURS_ONES, minutes % 10, show);
  tfts.setDigit(HOURS_TENS, minutes / 10, show);
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
  if (state == done)
  {
    uint32_t c = (now / 125) % 2 ? COLOR_RED : 0; // 4 Hz strobe
    for (uint8_t led = 0; led < NUM_BACKLIGHT_LEDS; led++)
      backlights.setOverride(led, c);
    return;
  }

  uint32_t t = remaining();
  if (state == running && t <= FINAL_SECONDS_MS)
  {
    // Last seconds: all LEDs pulse faster and faster, orange to red.
    float urgency = 1.0f - float(t) / FINAL_SECONDS_MS;
    float hz = 1.0f + 5.0f * urgency;
    float pulse = 0.25f + 0.75f * fabsf(sinf(M_PI * hz * now / 1000.0f));
    uint32_t c = scaleColor(mixColor(COLOR_ORANGE, COLOR_RED, urgency), pulse);
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
    backlights.setOverride(HOURS_TENS - p, scaleColor(COLOR_ORANGE, level * breath));
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
