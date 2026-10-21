#include "OnlineData.h"
#include <ArduinoJson.h>

const char *const ONLINE_TOPICS = "tubehax/online/#";

static OnlineData shared;
static volatile uint32_t version = 0;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;

static OnlineData::Health health(const char *s)
{
  if (s == nullptr)
    return OnlineData::ok;
  // Indicator: none | minor | major | critical | maintenance
  // Component: operational | degraded_performance | partial_outage | major_outage | under_maintenance
  if (strcmp(s, "minor") == 0 || strcmp(s, "degraded_performance") == 0)
    return OnlineData::minor;
  if (strcmp(s, "major") == 0 || strcmp(s, "partial_outage") == 0)
    return OnlineData::major;
  if (strcmp(s, "critical") == 0 || strcmp(s, "major_outage") == 0)
    return OnlineData::critical;
  if (strcmp(s, "maintenance") == 0 || strcmp(s, "under_maintenance") == 0)
    return OnlineData::maintenance;
  return OnlineData::ok;
}

bool OnlineDataHandle(const char *topic, const uint8_t *payload, size_t length)
{
  static const char prefix[] = "tubehax/online/";
  if (strncmp(topic, prefix, sizeof(prefix) - 1) != 0)
    return false;
  const char *kind = topic + sizeof(prefix) - 1;

  // ~800 bytes: static so the network task's stack isn't burdened; only this task writes it.
  static OnlineData d;
  OnlineDataGet(d);
  JsonDocument doc;
  if (length > 0 && deserializeJson(doc, payload, length))
  {
    Serial.printf("Online: bad JSON on %s\n", topic);
    return true;
  }

  if (strcmp(kind, "status") == 0)
  {
    d.has_status = length > 0;
    d.indicator = health(doc["indicator"]);
    strlcpy(d.description, doc["description"] | "", sizeof(d.description));
    d.component_count = 0;
    for (JsonArrayConst c : doc["components"].as<JsonArrayConst>())
    {
      if (d.component_count == OnlineData::MAX_COMPONENTS)
        break;
      strlcpy(d.component_name[d.component_count], c[0] | "", sizeof(d.component_name[0]));
      d.component_health[d.component_count++] = health(c[1]);
    }
    JsonVariantConst incident = doc["incident"];
    d.has_incident = !incident.isNull();
    strlcpy(d.incident_name, incident["name"] | "", sizeof(d.incident_name));
    strlcpy(d.incident_status, incident["status"] | "", sizeof(d.incident_status));
    d.incident_impact = health(incident["impact"]);
    d.incident_started = incident["started"] | 0;
    JsonArrayConst days = doc["days"];
    for (uint8_t i = 0; i < OnlineData::STATUS_DAYS; i++)
      d.days[i] = days[i] | 0;
    strlcpy(d.last_incident_name, doc["last_incident"]["name"] | "", sizeof(d.last_incident_name));
    d.last_incident_at = doc["last_incident"]["at"] | 0;
    d.status_fetched = doc["fetched_at"] | 0;
  }
  else if (strcmp(kind, "usage") == 0)
  {
    d.has_usage = length > 0;
    const char *const keys[] = {"five_hour", "seven_day", "model_week"};
    const char *const names[] = {"5H", "7D", "FABLE"};
    for (uint8_t i = 0; i < 3; i++)
    {
      JsonVariantConst w = doc[keys[i]];
      OnlineData::Limit &l = d.limits[i];
      l.valid = !w["pct"].isNull();
      strlcpy(l.name, w["name"] | names[i], sizeof(l.name));
      l.pct = w["pct"] | 0.0f;
      l.resets_at = w["resets_at"] | 0;
    }
    d.usage_fetched = doc["fetched_at"] | 0;
  }
  else if (strcmp(kind, "release") == 0)
  {
    d.has_release = length > 0;
    strlcpy(d.version, doc["version"] | "", sizeof(d.version));
    d.published_at = doc["published_at"] | 0;
    JsonArrayConst days = doc["days"];
    for (uint8_t i = 0; i < OnlineData::RELEASE_DAYS; i++)
      d.release_days[i] = days[i] | 0;
    d.note_count = 0;
    for (JsonVariantConst note : doc["notes"].as<JsonArrayConst>())
      if (d.note_count < OnlineData::MAX_NOTES)
        strlcpy(d.notes[d.note_count++], note | "", sizeof(d.notes[0]));
    d.release_fetched = doc["fetched_at"] | 0;
  }
  else
    return true;

  portENTER_CRITICAL(&lock);
  shared = d;
  version = version + 1;
  portEXIT_CRITICAL(&lock);
  return true;
}

uint32_t OnlineDataVersion()
{
  return version;
}

void OnlineDataGet(OnlineData &out)
{
  portENTER_CRITICAL(&lock);
  out = shared;
  portEXIT_CRITICAL(&lock);
}
