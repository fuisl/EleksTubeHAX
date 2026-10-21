// Renders Claude mode on the desktop: the real ClaudeApp / Clawd / Pixel code against stub displays,
// with sample online data, written as one PPM image of the six tubes (left to right). See run.sh.
#include <cstdlib>
#include <ctime>
#include "ClaudeApp.h"
#include "Clock.h"
#include "TFTs.h"

TFTs tfts;
Clock uclock;
static uint32_t fake_ms = 0;
static time_t fake_utc = 0;
static OnlineData fake;

uint32_t millis() { return fake_ms; }
long random(long n) { return n > 0 ? std::rand() % n : 0; }
long random(long a, long b) { return a + random(b - a); }
timeStatus_t timeStatus() { return timeSet; }
time_t now() { return fake_utc; }
static std::tm local() { time_t t = fake_utc + uclock.getTimeZoneOffset(); std::tm tm; gmtime_r(&t, &tm); return tm; }
int weekday(time_t t) { std::tm tm; gmtime_r(&t, &tm); return tm.tm_wday + 1; }
uint8_t Clock::getHour() { return local().tm_hour; }
uint8_t Clock::getMinute() { return local().tm_min; }
uint8_t Clock::getSecond() { return local().tm_sec; }
uint8_t Clock::getDay() { return local().tm_mday; }
uint32_t OnlineDataVersion() { return 1; }
void OnlineDataGet(OnlineData &out) { out = fake; }

static void sample(bool incident)
{
  fake = OnlineData();
  fake.has_status = true;
  fake.indicator = incident ? OnlineData::minor : OnlineData::ok;
  const char *names[] = {"claude.ai", "Console", "API", "Code", "Cowork", "Gov"};
  fake.component_count = 6;
  for (int i = 0; i < 6; i++)
  {
    strlcpy(fake.component_name[i], names[i], sizeof(fake.component_name[i]));
    fake.component_health[i] = incident && (i == 2 || i == 3) ? OnlineData::minor : OnlineData::ok;
  }
  uint8_t days[30] = {2, 0, 0, 0, 0, 0, 0, 2, 0, 1, 0, 0, 0, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  memcpy(fake.days, days, 30);
  if (incident)
  {
    fake.days[29] = 1;
    fake.has_incident = true;
    strlcpy(fake.incident_name, "Elevated errors for Claude Opus 5.5", sizeof(fake.incident_name));
    strlcpy(fake.incident_status, "identified", sizeof(fake.incident_status));
    fake.incident_started = fake_utc - 23 * 60;
  }
  strlcpy(fake.last_incident_name, "Elevated errors on platform.claude.com", sizeof(fake.last_incident_name));
  fake.last_incident_at = fake_utc - 14 * 86400;
  fake.status_fetched = fake_utc - 60;
  fake.has_usage = true;
  float pct[] = {23, 61, 88};
  uint32_t resets[] = {2 * 3600 + 14 * 60, 3 * 86400 + 4 * 3600, 3 * 86400 + 4 * 3600};
  for (int i = 0; i < 3; i++)
    fake.limits[i].valid = true, fake.limits[i].pct = pct[i], fake.limits[i].resets_at = fake_utc + resets[i];
  strlcpy(fake.limits[2].name, "FABLE", sizeof(fake.limits[2].name));
  fake.has_release = true;
  strlcpy(fake.version, "2.1.306", sizeof(fake.version));
  fake.published_at = fake_utc - 9 * 3600;
  uint8_t rel[14] = {2, 1, 0, 0, 0, 0, 2, 1, 1, 0, 1, 1, 1, 1};
  memcpy(fake.release_days, rel, 14);
  fake.note_count = 1;
  strlcpy(fake.notes[0], "Fixed claude -p inside cloud sessions reporting \"Not logged in\"", sizeof(fake.notes[0]));
}

static void write(const char *path)
{
  const int scale = 2, gap = 24, W = (6 * TFT_WIDTH + 7 * gap) * scale, H = (TFT_HEIGHT + 2 * gap) * scale;
  FILE *f = fopen(path, "wb");
  fprintf(f, "P6 %d %d 255\n", W, H);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++)
    {
      int px = x / scale, py = y / scale - gap, slot = (px - gap) / (TFT_WIDTH + gap), tx = (px - gap) % (TFT_WIDTH + gap);
      uint8_t rgb[3] = {22, 20, 18}; // Bezel
      if (px >= gap && slot < 6 && tx < TFT_WIDTH && py >= 0 && py < TFT_HEIGHT)
      {
        uint16_t c = tfts.fb[HOURS_TENS - slot][py][tx];
        rgb[0] = (c >> 11) << 3, rgb[1] = ((c >> 5) & 0x3F) << 2, rgb[2] = (c & 0x1F) << 3;
      }
      fwrite(rgb, 1, 3, f);
    }
  fclose(f);
}

int main(int argc, char **argv)
{
  const char *out = argc > 1 ? argv[1] : ".";
  struct Scene
  {
    const char *name;
    bool incident;
    uint8_t dimming;
    uint32_t ms;
  } scenes[] = {{"normal", false, 255, 700}, {"typing", false, 255, 9800}, {"incident", true, 255, 3100},
                {"night", false, 110, 5000}};
  for (const Scene &s : scenes)
  {
    std::srand(7);
    fake_utc = 1792586100; // 2026-10-21 19:15 local
    tfts = TFTs();
    tfts.dimming = s.dimming;
    sample(s.incident);
    ClaudeApp app;
    fake_ms = 1;
    app.enter();
    for (fake_ms = 1; fake_ms <= s.ms; fake_ms += 20) // Run the loop like the firmware (20 ms)
    {
      fake_utc = 1792586100 + fake_ms / 1000;
      app.loop();
    }
    char path[256];
    snprintf(path, sizeof(path), "%s/claude_%s.ppm", out, s.name);
    write(path);
    printf("%s\n", path);
  }
}
