#include "ClaudeData.h"
#include <ArduinoJson.h>

const char *const CLAUDE_TOPICS = "tubehax/claude/#";

static ClaudeData shared;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;

static ClaudeData::State parseState(const char *s)
{
  if (s == nullptr)
    return ClaudeData::unknown;
  if (strcmp(s, "working") == 0 || strcmp(s, "tool") == 0)
    return ClaudeData::working;
  if (strcmp(s, "waiting") == 0)
    return ClaudeData::waiting;
  if (strcmp(s, "done") == 0)
    return ClaudeData::done;
  return ClaudeData::idle;
}

bool ClaudeDataHandle(const char *topic, const uint8_t *payload, size_t length)
{
  static const char prefix[] = "tubehax/claude/";
  if (strncmp(topic, prefix, sizeof(prefix) - 1) != 0)
    return false;
  const char *kind = topic + sizeof(prefix) - 1;

  ClaudeData d;
  ClaudeDataGet(d);
  JsonDocument doc;
  if (length == 0) // Retained message deleted: forget that part
  {
    if (strcmp(kind, "state") == 0)
      d.has_state = false, d.state = ClaudeData::unknown;
    else if (strcmp(kind, "metrics") == 0)
      d.has_metrics = false;
    else if (strcmp(kind, "limits") == 0)
      d.has_limits = false;
  }
  else if (deserializeJson(doc, payload, length))
  {
    Serial.printf("Claude: bad JSON on %s\n", topic);
    return true;
  }
  // Parsed into a local copy, then swapped in under the lock (the main loop may be reading).
  else if (strcmp(kind, "state") == 0)
  {
    d.has_state = true;
    d.state = parseState(doc["state"]);
    strlcpy(d.tool, doc["tool"] | "", sizeof(d.tool));
    strlcpy(d.model, doc["model"] | "", sizeof(d.model));
    strlcpy(d.project, doc["project"] | "", sizeof(d.project));
    d.since = doc["since"] | 0.0;
    d.work_started = doc["work_started"] | 0.0;
    d.work_ended = doc["work_ended"] | 0.0;
    d.sessions = doc["sessions"] | 0;
  }
  else if (strcmp(kind, "metrics") == 0)
  {
    d.has_metrics = true;
    d.ctx_pct = doc["ctx_pct"] | 0.0f;
    d.ctx_size = doc["ctx_size"] | 0;
    d.tokens_in = doc["tokens_in"] | 0;
    d.tokens_out = doc["tokens_out"] | 0;
    d.cost_usd = doc["cost_usd"].isNull() ? NAN : doc["cost_usd"].as<float>(); // Unknown without a status line
    d.duration_s = (doc["duration_ms"] | 0.0) / 1000.0;
    d.sampled_at = doc["sampled_at"] | 0.0;
    d.lines_added = doc["lines_added"].isNull() ? -1 : doc["lines_added"].as<int32_t>();
    d.lines_removed = doc["lines_removed"].isNull() ? -1 : doc["lines_removed"].as<int32_t>();
  }
  else if (strcmp(kind, "limits") == 0)
  {
    d.has_limits = !doc["five_hour"].isNull() || !doc["seven_day"].isNull();
    d.five_pct = doc["five_hour"]["pct"] | 0.0f;
    d.five_resets = doc["five_hour"]["resets_at"] | 0.0;
    d.seven_pct = doc["seven_day"]["pct"] | 0.0f;
    d.seven_resets = doc["seven_day"]["resets_at"] | 0.0;
  }
  else
    return true; // Other claude topics: not ours

  portENTER_CRITICAL(&lock);
  d.version = shared.version + 1;
  shared = d;
  portEXIT_CRITICAL(&lock);
  return true;
}

void ClaudeDataGet(ClaudeData &out)
{
  portENTER_CRITICAL(&lock);
  out = shared;
  portEXIT_CRITICAL(&lock);
}
