#ifndef CLAUDE_APP_H
#define CLAUDE_APP_H

/*
 * Claude mode: an all-pixel Claude desk, from public information only (OnlineData.h, fed by homelab/feeds).
 * Tubes from the left:
 *   1  Clawd, the animated orange block (Clawd.h)
 *   2  spinner + CLI-style status verb ("Combobulating..."), or what Anthropic is doing about an incident
 *   3  the time in big pixel digits, date, seconds bar
 *   4  Anthropic status: overall icon + word, one row per component
 *   5  30-day incident strip; the open incident or the last one
 *   6  Claude Code: latest version, age, 14-day release strip, first change
 * Buttons: LEFT tap pokes Clawd, RIGHT tap picks a new verb. Backlights are left alone.
 */
#include "App.h"
#include "Clawd.h"
#include "OnlineData.h"
#include "Pixel.h"

class ClaudeApp : public App
{
public:
  const char *name() const override { return "claude"; }
  void enter() override;
  void exit() override { Pixel::releaseFrame(); } // 16 KB back, e.g. for the countdown's cache
  void loop() override;
  bool onButton(const ButtonEvent &event) override;

private:
  OnlineData data;
  uint32_t data_version = UINT32_MAX;
  String drawn[NUM_DIGITS]; // Content signature per tube; redrawn when it changes
  bool force_next = true;
  Clawd clawd;
  Pixel::Spinner spinner;
  const char *verb = "Thinking";
  uint32_t verb_since = 0;
  int8_t seconds_shown = -1;

  void draw(bool force);
  void drawClawd(bool force);
  void drawVerb(bool force);
  void drawTime(bool force);
  void drawStatus(bool force);
  void drawHistory(bool force);
  void drawRelease(bool force);
  void pickVerb();
  bool needsDraw(uint8_t tube, const String &signature, bool force);
};

#endif // CLAUDE_APP_H
