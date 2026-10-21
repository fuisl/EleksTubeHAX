#ifndef CLAUDE_APP_H
#define CLAUDE_APP_H

/*
 * Claude mode: what Claude Code is doing, from the data tubehax-bridge publishes (ClaudeData.h).
 * Times are nixie digits; everything else uses the Claude CLI pixel theme (Pixel.h).
 *
 * Pages (LEFT/RIGHT tap), tube 1 = leftmost:
 *   Live     1: spinner (working) / ? (waiting) / ✻ (idle)   2: state, tool, model   3-6: work timer MM SS
 *   Plan     1: window + usage % + gauge   2-6: time until reset, H MM SS (5 h) or D HH MM (7 days)
 *            RIGHT hold switches between the 5-hour and the 7-day window
 *   Context  1: label + gauge   2-4: used % in big pixels   5: input tokens   6: output tokens
 *   Session  1: cost, lines +/-   2-6: session time H MM SS
 * Auto-switch (setting auto_claude): when the clock shows the time and Claude asks for permission or
 * finishes a turn, it switches to Live for 60 s (any button keeps it there).
 * Backlights (only while no countdown runs): Live breathes while working, steady amber while waiting;
 * Plan shows usage as a bar. All in the clock's own backlight color except the amber.
 */
#include "App.h"
#include "ClaudeData.h"
#include "Pixel.h"

class ClaudeApp : public App
{
public:
  const char *name() const override { return "claude"; }
  void enter() override;
  void exit() override { auto_shown_at = 0; }
  void loop() override;
  void background() override; // Auto-switch: from the clock to Live when Claude needs you or finishes
  bool onButton(const ButtonEvent &event) override;

private:
  enum Page : uint8_t
  {
    live,
    plan,
    context,
    session,
    PAGE_COUNT
  };
  Page page = live;
  bool plan_week = false; // Plan page: 7-day instead of 5-hour window
  ClaudeData data;
  String drawn[NUM_DIGITS]; // What each pixel tube shows; redrawn only when it changes
  Pixel::Spinner spinner;
  bool force_next = true;
  uint32_t seen_version = 0;
  ClaudeData::State seen_state = ClaudeData::unknown;
  App *auto_from = nullptr;   // App to return to after an automatic switch
  uint32_t auto_shown_at = 0; // millis() of the automatic switch; 0: shown by the user

  void draw(bool force);
  void drawLive(bool force);
  void drawPlan(bool force);
  void drawContext(bool force);
  void drawSession(bool force);
  void updateBacklights();

  // Pixel tube helpers: true when the tube must be redrawn because its content signature changed.
  bool needsDraw(uint8_t tube, const String &signature, bool force);
  void nixie(uint8_t tube, int16_t value, bool force); // value < 0: blank
  void blankAll(bool force);
};

#endif // CLAUDE_APP_H
