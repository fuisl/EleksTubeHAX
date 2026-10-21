#include "ClaudeApp.h"
#include <TimeLib.h>
#include "Clock.h"
#include "TFTs.h"

// Tubes from the left: 1 = HOURS_TENS ... 6 = SECONDS_ONES
static const uint8_t TUBE1 = HOURS_TENS, TUBE2 = HOURS_ONES, TUBE3 = MINUTES_TENS, TUBE4 = MINUTES_ONES,
                     TUBE5 = SECONDS_TENS, TUBE6 = SECONDS_ONES;

static const uint32_t VERB_MS = 9000;    // A new verb this often
static const uint32_t STALE_S = 2 * 3600; // No feed update for this long: say so (it republishes hourly)

// Claude Code's spinner verbs (a selection).
static const char *const VERBS[] = {
    "Thinking", "Crunching", "Combobulating", "Pondering", "Cogitating", "Noodling", "Percolating",
    "Ruminating", "Simmering", "Brewing", "Conjuring", "Mulling", "Musing", "Vibing", "Clauding",
    "Spelunking", "Synthesizing", "Marinating", "Finagling", "Reticulating", "Cerebrating", "Hatching",
    "Forging", "Tinkering", "Whirring", "Moseying", "Schlepping", "Smooshing", "Stewing", "Honking"};

static uint32_t utcNow()
{
  return timeStatus() == timeNotSet ? 0 : uint32_t(now());
}

// "5M AGO", "3H AGO", "2D AGO"
static String ago(uint32_t then)
{
  uint32_t t = utcNow();
  if (then == 0 || t < then)
    return "";
  uint32_t s = t - then;
  char buf[12];
  if (s < 3600)
    snprintf(buf, sizeof(buf), "%luM AGO", (unsigned long)max(s / 60, uint32_t(1)));
  else if (s < 48 * 3600)
    snprintf(buf, sizeof(buf), "%luH AGO", (unsigned long)(s / 3600));
  else
    snprintf(buf, sizeof(buf), "%luD AGO", (unsigned long)(s / 86400));
  return buf;
}

static uint32_t healthColor(OnlineData::Health h)
{
  switch (h)
  {
  case OnlineData::ok:
    return Pixel::ORANGE;
  case OnlineData::minor:
  case OnlineData::maintenance:
    return Pixel::AMBER;
  default:
    return Pixel::RED;
  }
}

void ClaudeApp::enter()
{
  tfts.show_status_overlays = false;
  force_next = true;
  if (verb_since == 0)
    pickVerb();
}

bool ClaudeApp::needsDraw(uint8_t tube, const String &signature, bool force)
{
  if (!force && drawn[tube] == signature && tfts.getDigit(tube) == TFTs::custom)
    return false;
  drawn[tube] = signature;
  Pixel::begin(tube);
  Pixel::clear();
  return true;
}

void ClaudeApp::pickVerb()
{
  const char *next;
  do
    next = VERBS[random(sizeof(VERBS) / sizeof(VERBS[0]))];
  while (next == verb);
  verb = next;
  verb_since = millis();
  clawd.lookRight(); // Clawd glances at the new word
}

void ClaudeApp::loop()
{
  bool force = force_next;
  force_next = false;
  if (OnlineDataVersion() != data_version)
  {
    data_version = OnlineDataVersion();
    OnlineDataGet(data);
  }
  draw(force);
}

void ClaudeApp::draw(bool force)
{
  drawClawd(force);
  drawVerb(force);
  drawTime(force);
  drawStatus(force);
  drawHistory(force);
  drawRelease(force);
}

void ClaudeApp::drawClawd(bool force)
{
  if (needsDraw(TUBE1, "clawd", force))
  {
    clawd.redraw();
    Pixel::textCentered(206, "CLAUDE", 3, Pixel::ORANGE_DIM);
  }
  else
    tfts.selectCustom(TUBE1);
  Clawd::Mood mood = tfts.dimming < 255                                     ? Clawd::sleepy
                     : data.has_incident || data.indicator != OnlineData::ok ? Clawd::worried
                                                                             : Clawd::normal;
  clawd.update(millis(), mood);
}

void ClaudeApp::drawVerb(bool force)
{
  uint32_t ms = millis();
  // During an incident, show what Anthropic is doing about it instead of a playful verb.
  const char *word = verb;
  uint32_t color = Pixel::ORANGE;
  if (data.has_incident)
  {
    const char *s = data.incident_status;
    word = strcmp(s, "identified") == 0 ? "Fixing" : strcmp(s, "monitoring") == 0 ? "Monitoring"
                                                                                    : "Investigating";
    color = Pixel::AMBER;
  }
  else if (ms - verb_since > VERB_MS)
  {
    pickVerb();
    word = verb;
  }

  bool redrawn = needsDraw(TUBE2, word, force);
  if (redrawn)
  {
    // Scale 3 fits 7 characters: longer words split after 6 with a hyphen ("Reticu-" / "lating").
    size_t len = strlen(word);
    if (len <= 7)
      Pixel::textCentered(116, word, 3, color);
    else
    {
      char first[8];
      strlcpy(first, word, 7);
      strlcat(first, "-", sizeof(first));
      Pixel::textCentered(100, first, 3, color);
      Pixel::textCentered(130, word + 6, Pixel::fitScale(word + 6, 3), color);
    }
  }
  else
    tfts.selectCustom(TUBE2);
  spinner.draw(14, Pixel::Spinner::frameAt(ms), color, redrawn, 7);

  // Animated dots and the CLI-style elapsed time: "(12s)".
  static uint8_t dots_shown = 255;
  static uint32_t secs_shown = UINT32_MAX;
  uint8_t dots = (ms / 400) % 4;
  if (dots != dots_shown || redrawn)
  {
    tfts.fillRect(0, 168, Pixel::WIDTH, 21, TFT_BLACK);
    for (uint8_t i = 0; i < dots; i++)
      tfts.fillRect(46 + i * 16, 180, 9, 9, Pixel::color(color));
    dots_shown = dots;
  }
  uint32_t t = utcNow();
  uint32_t secs = data.has_incident ? (t > data.incident_started ? t - data.incident_started : 0)
                                    : (ms - verb_since) / 1000;
  if (secs != secs_shown || redrawn)
  {
    char buf[16];
    if (secs < 600)
      snprintf(buf, sizeof(buf), "(%lus)", (unsigned long)secs);
    else
      snprintf(buf, sizeof(buf), "(%lum)", (unsigned long)(secs / 60));
    Pixel::textCentered(210, buf, 2, Pixel::MUTED);
    secs_shown = secs;
  }
}

void ClaudeApp::drawTime(bool force)
{
  char hh[3], mm[3], date[12];
  snprintf(hh, sizeof(hh), "%02u", uclock.getHour());
  snprintf(mm, sizeof(mm), "%02u", uclock.getMinute());
  static const char *const DAYS[] = {"", "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  time_t local = uclock.loop_time + uclock.getTimeZoneOffset();
  snprintf(date, sizeof(date), "%s %u", DAYS[weekday(local)], uclock.getDay());

  if (needsDraw(TUBE3, String(hh) + mm + date, force))
  {
    Pixel::textCentered(22, hh, 10, Pixel::ORANGE);
    Pixel::textCentered(116, mm, 10, Pixel::ORANGE);
    Pixel::textCentered(200, date, 2, Pixel::MUTED);
    seconds_shown = -1;
  }
  else
    tfts.selectCustom(TUBE3);

  // Colon between the rows (blinking) and a seconds bar along the bottom.
  int8_t s = uclock.getSecond();
  if (s != seconds_shown)
  {
    uint16_t on = s % 2 ? Pixel::color(Pixel::ORANGE_DIM) : TFT_BLACK;
    tfts.fillRect(52, 98, 9, 9, on);
    tfts.fillRect(74, 98, 9, 9, on);
    if (s < seconds_shown || seconds_shown < 0)
      tfts.fillRect(8, 226, 120, 5, Pixel::color(Pixel::UNLIT));
    tfts.fillRect(8, 226, 2 * s, 5, Pixel::color(Pixel::ORANGE_DIM));
    seconds_shown = s;
  }
}

// "2h 14m", "3d 4h"
static String until(uint32_t then)
{
  uint32_t t = utcNow();
  if (then == 0 || t == 0)
    return "";
  uint32_t s = then > t ? then - t : 0;
  char buf[12];
  if (s >= 86400)
    snprintf(buf, sizeof(buf), "%lud %luh", (unsigned long)(s / 86400), (unsigned long)(s / 3600 % 24));
  else
    snprintf(buf, sizeof(buf), "%luh %lum", (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60));
  return buf;
}

void ClaudeApp::drawStatus(bool force)
{
  bool stale = data.has_status && utcNow() > data.status_fetched + STALE_S;
  String sig = String(data.has_status) + stale + data.indicator + data.has_usage;
  for (uint8_t i = 0; i < data.component_count; i++)
    sig += data.component_health[i];
  for (const OnlineData::Limit &l : data.limits)
    sig += String(l.valid) + int(l.pct) + l.name + until(l.resets_at); // "until" changes once a minute
  if (!needsDraw(TUBE4, sig, force))
    return;

  // Overall status: small icon + word
  static const char *const CHECK[] = {".........", "........#", ".......##", "......##.", "#....##..",
                                      "##..##...", ".####....", "..##.....", "........."};
  static const char *const BANG[] = {"...###...", "...###...", "...###...", "...###...", "...###...",
                                     ".........", "...###...", "...###...", "........."};
  static const char *const QUESTION[] = {"..#####..", ".##...##.", "......##.", "....###..", "...##....",
                                         ".........", "...##....", "...##....", "........."};
  OnlineData::Health h = data.indicator;
  bool unknown = !data.has_status || stale;
  uint32_t color = unknown ? Pixel::MUTED : healthColor(h);
  const Pixel::Ink ink[] = {{'#', color}};
  Pixel::art(10, 8, 5, unknown ? QUESTION : h == OnlineData::ok ? CHECK : BANG, 9, ink, 1);
  static const char *const WORDS[] = {"ALL", "MINOR", "MAJOR", "OUTAGE", "MAINT"}; // 6 chars fit beside the icon
  static const char *const WORDS2[] = {"GOOD", "ISSUE", "ISSUE", "", ""};
  const char *w1 = !data.has_status ? "NO" : stale ? "STALE" : WORDS[h];
  const char *w2 = !data.has_status ? "FEED" : stale ? "" : WORDS2[h];
  Pixel::text(62, 10, w1, 2, color);
  Pixel::text(62, 30, w2, 2, color);

  // Plan limits: name + percent, bar, time to reset
  for (uint8_t i = 0; i < 3; i++)
  {
    const OnlineData::Limit &l = data.limits[i];
    int16_t y = 62 + i * 50;
    const char *name = l.name[0] ? l.name : (i == 0 ? "5H" : i == 1 ? "7D" : "FABLE");
    Pixel::text(6, y, name, 2, Pixel::TEXT);
    char pct[8] = "--";
    if (l.valid)
      snprintf(pct, sizeof(pct), "%d%%", int(l.pct + 0.5f));
    Pixel::text(129 - Pixel::textWidth(pct, 2), y, pct, 2, l.valid ? Pixel::ORANGE : Pixel::MUTED);
    float f = l.valid ? constrain(l.pct / 100.0f, 0.0f, 1.0f) : 0;
    uint32_t fill = l.pct >= 95 ? Pixel::RED : l.pct >= 80 ? Pixel::AMBER : Pixel::ORANGE;
    tfts.fillRect(6, y + 18, 123, 8, Pixel::color(Pixel::UNLIT));
    if (f > 0)
      tfts.fillRect(6, y + 18, max(int16_t(2), int16_t(123 * f)), 8, Pixel::color(fill));
    String reset = l.valid ? until(l.resets_at) : "";
    if (reset.length())
      Pixel::text(6, y + 30, reset.c_str(), 1, Pixel::MUTED);
  }

  // Components: one square each, with how many are up
  uint8_t up = 0;
  for (uint8_t i = 0; i < data.component_count; i++)
    up += data.component_health[i] == OnlineData::ok;
  if (data.component_count)
  {
    char summary[12];
    snprintf(summary, sizeof(summary), "%u/%u UP", up, data.component_count);
    Pixel::text(6, 214, summary, 1, Pixel::MUTED);
    for (uint8_t i = 0; i < data.component_count; i++)
      tfts.fillRect(64 + i * 11, 212, 9, 11, Pixel::color(healthColor(data.component_health[i])));
  }
}

void ClaudeApp::drawHistory(bool force)
{
  String when = data.has_incident ? "" : ago(data.last_incident_at); // Changes at most once a minute
  String sig = String(data.has_status) + data.has_incident + data.incident_name + when;
  for (uint8_t d : data.days)
    sig += char('0' + d);
  if (!needsDraw(TUBE5, sig, force))
    return;

  Pixel::textCentered(8, "30 DAYS", 2, Pixel::ORANGE_DIM);
  // One bar per day, today on the right; height and color by the worst incident that day.
  static const uint8_t HEIGHT[] = {5, 26, 52, 76};
  static const uint32_t COLOR[] = {Pixel::ORANGE_DIM, Pixel::AMBER, Pixel::ORANGE, Pixel::RED};
  for (uint8_t i = 0; i < OnlineData::STATUS_DAYS; i++)
  {
    uint8_t impact = min(data.days[i], uint8_t(3));
    tfts.fillRect(8 + i * 4, 104 - HEIGHT[impact], 3, HEIGHT[impact], Pixel::color(COLOR[impact]));
  }
  tfts.fillRect(8, 106, 119, 1, Pixel::color(Pixel::UNLIT));

  if (!data.has_status)
    return;
  if (data.has_incident)
  {
    Pixel::textCentered(118, "NOW", 3, Pixel::AMBER);
    Pixel::textWrapped(4, 148, 127, data.incident_name, 2, Pixel::TEXT, 5, true);
  }
  else if (data.last_incident_at)
  {
    Pixel::textCentered(118, "LAST", 2, Pixel::MUTED);
    Pixel::textCentered(138, when.c_str(), Pixel::fitScale(when.c_str(), 3), Pixel::ORANGE);
    Pixel::textWrapped(4, 168, 127, data.last_incident_name, 2, Pixel::MUTED, 4, true);
  }
}

void ClaudeApp::drawRelease(bool force)
{
  String when = ago(data.published_at);
  String sig = String(data.has_release) + data.version + when + (data.note_count ? data.notes[0] : "");
  for (uint8_t d : data.release_days)
    sig += char('0' + d);
  if (!needsDraw(TUBE6, sig, force))
    return;

  Pixel::textCentered(8, "CLAUDE CODE", 2, Pixel::ORANGE_DIM);
  if (!data.has_release)
  {
    Pixel::textCentered(110, "NO FEED", 2, Pixel::MUTED);
    return;
  }
  Pixel::textCentered(32, data.version, Pixel::fitScale(data.version, 3), Pixel::ORANGE);
  Pixel::textCentered(62, when.c_str(), 2, Pixel::MUTED);

  // Releases per day for 14 days, today on the right.
  uint8_t total = 0;
  for (uint8_t i = 0; i < OnlineData::RELEASE_DAYS; i++)
  {
    uint8_t n = min(data.release_days[i], uint8_t(4));
    total += data.release_days[i];
    int16_t h = n ? n * 13 : 3;
    tfts.fillRect(5 + i * 9, 132 - h, 7, h, Pixel::color(n ? Pixel::ORANGE : Pixel::UNLIT));
  }
  char count[16];
  snprintf(count, sizeof(count), "%u IN 14D", total);
  Pixel::textCentered(140, count, 2, Pixel::MUTED);
  if (data.note_count)
    Pixel::textWrapped(4, 166, 127, data.notes[0], 2, Pixel::TEXT, 4, false);
}

bool ClaudeApp::onButton(const ButtonEvent &event)
{
  if (event.gesture != Gesture::Tap || (event.button != ButtonId::Left && event.button != ButtonId::Right))
    return false;
  if (event.button == ButtonId::Left)
    clawd.poke();
  else
    pickVerb();
  return true;
}
