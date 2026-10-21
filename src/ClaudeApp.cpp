#include "ClaudeApp.h"
#include <TimeLib.h>
#include "Backlights.h"
#include "CountdownApp.h"
#include "StoredConfig.h"
#include "TFTs.h"

extern CountdownApp countdownApp; // main.cpp
extern StoredConfig stored_config;

static const uint32_t AUTO_SHOW_MS = 60000;

// Tubes from the left: 1 = HOURS_TENS ... 6 = SECONDS_ONES
static const uint8_t TUBE1 = HOURS_TENS, TUBE2 = HOURS_ONES, TUBE3 = MINUTES_TENS, TUBE4 = MINUTES_ONES,
                     TUBE5 = SECONDS_TENS, TUBE6 = SECONDS_ONES;
static const uint8_t NIXIE_TUBES[] = {TUBE2, TUBE3, TUBE4, TUBE5, TUBE6};

static uint32_t utcNow()
{
  return timeStatus() == timeNotSet ? 0 : uint32_t(now());
}

static uint32_t scaleColor(uint32_t color, float f)
{
  uint8_t r = ((color >> 16) & 0xFF) * f, g = ((color >> 8) & 0xFF) * f, b = (color & 0xFF) * f;
  return (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
}

// 123456 -> "123K", 1234567 -> "1.2M"
static String shortCount(uint32_t n)
{
  char buf[12];
  if (n >= 1000000)
    snprintf(buf, sizeof(buf), "%.1fM", n / 1e6);
  else if (n >= 10000)
    snprintf(buf, sizeof(buf), "%luK", (unsigned long)(n / 1000));
  else if (n >= 1000)
    snprintf(buf, sizeof(buf), "%.1fK", n / 1e3);
  else
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)n);
  return buf;
}

static String upper(const char *s, size_t max_len)
{
  String out(s);
  out.toUpperCase();
  return out.length() > max_len ? out.substring(0, max_len) : out;
}

void ClaudeApp::enter()
{
  tfts.show_status_overlays = false;
  tfts.preload_position = TUBE6;
  tfts.preload_step = 1;
  force_next = true;
  draw(true);
}

bool ClaudeApp::needsDraw(uint8_t tube, const String &signature, bool force)
{
  if (!force && drawn[tube] == signature && tfts.getDigit(tube) == TFTs::custom)
    return false;
  drawn[tube] = signature;
  Pixel::begin(tube);
  return true;
}

void ClaudeApp::nixie(uint8_t tube, int16_t value, bool force)
{
  drawn[tube] = "";
  tfts.setDigit(tube, value < 0 ? TFTs::blanked : uint8_t(value % 10), force ? TFTs::force : TFTs::yes);
}

void ClaudeApp::loop()
{
  draw(force_next);
  force_next = false;
  updateBacklights();
}

void ClaudeApp::draw(bool force)
{
  ClaudeDataGet(data);
  tfts.preload_step = page == plan ? -1 : 1; // Seconds count down to the reset, up everywhere else
  switch (page)
  {
  case live:
    drawLive(force);
    break;
  case plan:
    drawPlan(force);
    break;
  case context:
    drawContext(force);
    break;
  default:
    drawSession(force);
    break;
  }
}

void ClaudeApp::drawLive(bool force)
{
  uint32_t t = utcNow();

  // Tube 1: the spinner while working, ? while waiting, a still ✻ otherwise.
  if (data.state == ClaudeData::working)
  {
    if (needsDraw(TUBE1, "spin", force))
    {
      Pixel::clear();
      spinner.draw(60, Pixel::Spinner::frameAt(millis()), Pixel::ORANGE, true);
    }
    else
    {
      tfts.selectCustom(TUBE1);
      spinner.draw(60, Pixel::Spinner::frameAt(millis()), Pixel::ORANGE, false);
    }
  }
  else if (needsDraw(TUBE1, String("still") + data.state, force))
  {
    Pixel::clear();
    if (data.state == ClaudeData::waiting)
      Pixel::textCentered(64, "?", 16, Pixel::AMBER);
    else
      spinner.draw(60, 4, data.state == ClaudeData::done ? Pixel::ORANGE : Pixel::MUTED, true);
  }

  // Tube 2: state word, tool, model, project.
  static const char *const WORDS[] = {"NO DATA", "IDLE", "WORKING", "WAITING", "DONE"};
  static const uint32_t COLORS[] = {Pixel::MUTED, Pixel::MUTED, Pixel::ORANGE, Pixel::AMBER, Pixel::GREEN};
  const char *word = WORDS[data.state];
  String tool = data.state == ClaudeData::working ? upper(data.tool, 11) : "";
  String sig = String(word) + "|" + tool + "|" + data.model + "|" + data.project;
  if (needsDraw(TUBE2, sig, force))
  {
    Pixel::clear();
    Pixel::textCentered(24, word, 3, COLORS[data.state]);
    if (tool.length())
      Pixel::textCentered(66, tool.c_str(), 2, Pixel::TEXT);
    if (data.model[0])
      Pixel::textCentered(170, upper(data.model, 11).c_str(), 2, Pixel::ORANGE);
    if (data.project[0])
      Pixel::textCentered(200, upper(data.project, 11).c_str(), 2, Pixel::MUTED);
  }

  // Tubes 3-6: active work time, MM SS (HH MM from an hour on); frozen once the turn ended.
  if (data.work_started == 0 || t == 0)
  {
    for (uint8_t tube : {TUBE3, TUBE4, TUBE5, TUBE6})
      nixie(tube, -1, force);
    return;
  }
  uint32_t end = data.work_ended ? data.work_ended : t;
  uint32_t s = end > data.work_started ? end - data.work_started : 0;
  uint32_t hi = s >= 3600 ? (s / 3600) % 100 : s / 60, lo = s >= 3600 ? (s / 60) % 60 : s % 60;
  nixie(TUBE3, hi / 10, force);
  nixie(TUBE4, hi % 10, force);
  nixie(TUBE5, lo / 10, force);
  nixie(TUBE6, lo % 10, force);
}

void ClaudeApp::drawPlan(bool force)
{
  uint32_t t = utcNow();
  float pct = plan_week ? data.seven_pct : data.five_pct;
  uint32_t resets = plan_week ? data.seven_resets : data.five_resets;
  bool valid = data.has_limits && resets > 0 && t > 0;

  char pct_text[8];
  snprintf(pct_text, sizeof(pct_text), "%d%%", int(pct + 0.5f));
  if (needsDraw(TUBE1, String(plan_week) + valid + pct_text, force))
  {
    Pixel::clear();
    Pixel::textCentered(20, plan_week ? "7D" : "5H", 7, Pixel::ORANGE);
    if (valid)
    {
      Pixel::textCentered(100, pct_text, 4, Pixel::TEXT);
      Pixel::gauge(14, 160, 107, 40, 5, pct / 100.0f, Pixel::ORANGE, false);
    }
    else
      Pixel::textCentered(110, "NO DATA", 2, Pixel::MUTED);
  }

  if (!valid)
  {
    for (uint8_t tube : NIXIE_TUBES)
      nixie(tube, -1, force);
    return;
  }
  uint32_t s = resets > t ? resets - t : 0;
  if (!plan_week) // H MM SS
  {
    uint32_t h = min(s / 3600, uint32_t(9)), m = (s / 60) % 60, sec = s % 60;
    int16_t values[] = {int16_t(h), int16_t(m / 10), int16_t(m % 10), int16_t(sec / 10), int16_t(sec % 10)};
    for (uint8_t i = 0; i < 5; i++)
      nixie(NIXIE_TUBES[i], values[i], force);
  }
  else // D HH MM
  {
    uint32_t d = min(s / 86400, uint32_t(9)), h = (s / 3600) % 24, m = (s / 60) % 60;
    int16_t values[] = {int16_t(d), int16_t(h / 10), int16_t(h % 10), int16_t(m / 10), int16_t(m % 10)};
    for (uint8_t i = 0; i < 5; i++)
      nixie(NIXIE_TUBES[i], values[i], force);
  }
}

void ClaudeApp::drawContext(bool force)
{
  bool valid = data.has_metrics && data.ctx_size > 0;
  float pct = data.ctx_pct;

  if (needsDraw(TUBE1, String(valid) + int(pct), force))
  {
    Pixel::clear();
    Pixel::textCentered(20, "CTX", 5, Pixel::ORANGE);
    if (valid)
      Pixel::gauge(42, 80, 51, 140, 8, pct / 100.0f, Pixel::ORANGE, true);
    else
      Pixel::textCentered(110, "NO DATA", 2, Pixel::MUTED);
  }

  // Tubes 2-4: the percentage in big pixels, one character per tube.
  char digits[4] = {' ', ' ', ' ', 0};
  if (valid)
  {
    int p = constrain(int(pct + 0.5f), 0, 100);
    if (p == 100)
      digits[0] = '1', digits[1] = '0', digits[2] = '0';
    else
      digits[0] = p >= 10 ? '0' + p / 10 : ' ', digits[1] = '0' + p % 10, digits[2] = '%';
  }
  const uint8_t big_tubes[] = {TUBE2, TUBE3, TUBE4};
  for (uint8_t i = 0; i < 3; i++)
  {
    char ch[2] = {digits[i], 0};
    if (needsDraw(big_tubes[i], ch, force))
    {
      Pixel::clear();
      if (ch[0] != ' ')
        Pixel::textCentered(64, ch, 16, Pixel::ORANGE);
    }
  }

  // Tubes 5-6: input and output tokens.
  String in = valid ? shortCount(data.tokens_in) : "", out = valid ? shortCount(data.tokens_out) : "";
  if (needsDraw(TUBE5, in + "/" + data.ctx_size, force))
  {
    Pixel::clear();
    Pixel::textCentered(40, "IN", 4, Pixel::MUTED);
    if (valid)
    {
      Pixel::textCentered(100, in.c_str(), Pixel::fitScale(in.c_str(), 4), Pixel::TEXT);
      Pixel::textCentered(170, ("OF " + shortCount(data.ctx_size)).c_str(), 2, Pixel::MUTED);
    }
  }
  if (needsDraw(TUBE6, out, force))
  {
    Pixel::clear();
    Pixel::textCentered(40, "OUT", 4, Pixel::MUTED);
    if (valid)
      Pixel::textCentered(100, out.c_str(), Pixel::fitScale(out.c_str(), 4), Pixel::TEXT);
  }
}

void ClaudeApp::drawSession(bool force)
{
  uint32_t t = utcNow();
  bool valid = data.has_metrics;

  // Cost and lines come from the status line only; without it (VS Code panel) show the output tokens.
  char cost[12], added[12], removed[12];
  bool has_cost = !isnan(data.cost_usd), has_lines = data.lines_added >= 0;
  snprintf(cost, sizeof(cost), "$%.2f", has_cost ? data.cost_usd : 0.0f);
  snprintf(added, sizeof(added), "+%ld", (long)data.lines_added);
  snprintf(removed, sizeof(removed), "-%ld", (long)data.lines_removed);
  String out = shortCount(data.tokens_out);
  String sig = valid ? (has_cost ? String(cost) : out) + (has_lines ? String(added) + removed : "") : "none";
  if (needsDraw(TUBE1, sig, force))
  {
    Pixel::clear();
    if (!valid)
      Pixel::textCentered(110, "NO DATA", 2, Pixel::MUTED);
    else if (has_cost)
      Pixel::textCentered(24, cost, Pixel::fitScale(cost, 4), Pixel::ORANGE);
    else
    {
      Pixel::textCentered(24, "OUT", 3, Pixel::MUTED);
      Pixel::textCentered(56, out.c_str(), Pixel::fitScale(out.c_str(), 4), Pixel::ORANGE);
    }
    if (valid && has_lines)
    {
      Pixel::textCentered(110, added, Pixel::fitScale(added, 3), Pixel::GREEN);
      Pixel::textCentered(160, removed, Pixel::fitScale(removed, 3), Pixel::ROSE);
    }
  }

  if (!valid || t == 0)
  {
    for (uint8_t tube : NIXIE_TUBES)
      nixie(tube, -1, force);
    return;
  }
  // Session wall time at sampling, ticking on while the session is alive.
  bool alive = data.state != ClaudeData::idle && data.state != ClaudeData::unknown;
  uint32_t s = data.duration_s + (alive && t > data.sampled_at ? t - data.sampled_at : 0);
  uint32_t h = min(s / 3600, uint32_t(9)), m = (s / 60) % 60, sec = s % 60;
  int16_t values[] = {int16_t(h), int16_t(m / 10), int16_t(m % 10), int16_t(sec / 10), int16_t(sec % 10)};
  for (uint8_t i = 0; i < 5; i++)
    nixie(NIXIE_TUBES[i], values[i], force);
}

void ClaudeApp::updateBacklights()
{
  if (strcmp(countdownApp.stateName(), "stopped") != 0)
    return; // A running countdown owns the backlights
  uint32_t color = backlights.getColor();
  if (page == plan && data.has_limits)
  {
    float lit = NUM_BACKLIGHT_LEDS * (plan_week ? data.seven_pct : data.five_pct) / 100.0f;
    for (uint8_t p = 0; p < NUM_BACKLIGHT_LEDS; p++) // p = 0 is the leftmost tube
      backlights.setOverride(HOURS_TENS - p, scaleColor(color, constrain(lit - p, 0.0f, 1.0f)));
  }
  else if (page == live && data.state == ClaudeData::working)
  {
    float breath = 0.45f + 0.55f * (0.5f + 0.5f * cosf(2.0f * float(M_PI) * millis() / 3000.0f));
    for (uint8_t led = 0; led < NUM_BACKLIGHT_LEDS; led++)
      backlights.setOverride(led, scaleColor(color, breath));
  }
  else if (page == live && data.state == ClaudeData::waiting)
  {
    for (uint8_t led = 0; led < NUM_BACKLIGHT_LEDS; led++)
      backlights.setOverride(led, Pixel::AMBER);
  }
}

void ClaudeApp::background()
{
  if (auto_shown_at != 0 && apps.isActive(this) && millis() - auto_shown_at > AUTO_SHOW_MS &&
      data.state != ClaudeData::waiting)
  {
    auto_shown_at = 0;
    apps.switchTo(auto_from);
    return;
  }

  ClaudeData d;
  ClaudeDataGet(d);
  if (d.version == seen_version)
    return;
  bool first = seen_version == 0; // Retained state after boot: nothing new happened
  ClaudeData::State before = seen_state;
  seen_version = d.version;
  seen_state = d.state;
  if (first || d.state == before || !stored_config.config.display.auto_claude)
    return;

  bool needs_you = d.state == ClaudeData::waiting;
  bool finished = d.state == ClaudeData::done && before == ClaudeData::working;
  App *current = apps.active();
  if ((needs_you || finished) && current != nullptr && strcmp(current->name(), "clock") == 0)
  {
    auto_from = current;
    page = live;
    force_next = true;
    apps.switchTo(this);
    auto_shown_at = millis();
    Serial.printf("Claude: auto-switch (%s)\n", needs_you ? "waiting" : "done");
  }
  else if (auto_shown_at != 0 && apps.isActive(this))
    auto_shown_at = millis(); // Still happening: keep showing
}

bool ClaudeApp::onButton(const ButtonEvent &event)
{
  auto_shown_at = 0; // The user took over: stay

  if (event.button != ButtonId::Left && event.button != ButtonId::Right)
    return false;
  if (event.gesture == Gesture::Tap)
  {
    int8_t step = event.button == ButtonId::Right ? 1 : PAGE_COUNT - 1;
    page = Page((page + step) % PAGE_COUNT);
    force_next = true;
  }
  else if (event.gesture == Gesture::Hold && event.button == ButtonId::Right && page == plan)
  {
    plan_week = !plan_week;
    force_next = true;
  }
  return true;
}
