#ifndef ONLINE_DATA_H
#define ONLINE_DATA_H

/*
 * Public Claude information, fetched by the homelab feeder (homelab/feeds) and published retained:
 *   tubehax/online/status   {"indicator","description","components":[[name,status]...],"incident":{...}|null,
 *                            "days":[30 x 0..3],"last_incident":{"name","at"}|null,"fetched_at"}
 *   tubehax/online/release  {"version","published_at","days":[14 x count],"notes":[...],"fetched_at"}
 *   tubehax/online/usage    {"five_hour":{"pct","resets_at"},"seven_day":{...},"model_week":{"name","pct","resets_at"},
 *                            "fetched_at"}  (plan limits of the account; source not set up yet)
 * Times are UTC epoch seconds. Parsed on the network task, read on the main loop.
 */
#include <Arduino.h>

struct OnlineData
{
  static const uint8_t MAX_COMPONENTS = 6, STATUS_DAYS = 30, RELEASE_DAYS = 14, MAX_NOTES = 3;

  enum Health : uint8_t // Overall indicator and component status
  {
    ok,
    minor,       // degraded performance
    major,       // partial outage
    critical,    // major outage
    maintenance
  };

  // tubehax/online/status
  bool has_status = false;
  Health indicator = ok;
  char description[40] = "";
  uint8_t component_count = 0;
  char component_name[MAX_COMPONENTS][10] = {};
  Health component_health[MAX_COMPONENTS] = {};
  bool has_incident = false;
  char incident_name[92] = "";
  char incident_status[16] = ""; // investigating | identified | monitoring | resolved
  Health incident_impact = ok;
  uint32_t incident_started = 0;
  uint8_t days[STATUS_DAYS] = {}; // Worst incident impact per day, oldest first, today last
  char last_incident_name[92] = "";
  uint32_t last_incident_at = 0;
  uint32_t status_fetched = 0;

  // tubehax/online/usage: three plan limits (5 hours, 7 days, one model's week), percent used
  struct Limit
  {
    bool valid = false;
    char name[10] = "";
    float pct = 0;
    uint32_t resets_at = 0;
  };
  bool has_usage = false;
  Limit limits[3]; // 5H, 7D, model weekly
  uint32_t usage_fetched = 0;

  // tubehax/online/release
  bool has_release = false;
  char version[16] = "";
  uint32_t published_at = 0;
  uint8_t release_days[RELEASE_DAYS] = {}; // Releases per day, oldest first
  uint8_t note_count = 0;
  char notes[MAX_NOTES][82] = {};
  uint32_t release_fetched = 0;
};

extern const char *const ONLINE_TOPICS;
bool OnlineDataHandle(const char *topic, const uint8_t *payload, size_t length); // Network task

uint32_t OnlineDataVersion();       // Incremented on every change; cheap
void OnlineDataGet(OnlineData &out); // Main loop: copy of the latest data

#endif // ONLINE_DATA_H
