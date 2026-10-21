#ifndef CLAUDE_DATA_H
#define CLAUDE_DATA_H

/*
 * Latest Claude Code facts published by tubehax-bridge (bridge/, docs/DESIGN.md#mqtt-contract):
 *   tubehax/claude/state    {"state","tool","model","project","since","work_started","work_ended","sessions"}
 *   tubehax/claude/metrics  {"ctx_pct","ctx_size","tokens_in","tokens_out","cost_usd","duration_ms",
 *                            "lines_added","lines_removed","model","project","sampled_at"}
 *   tubehax/claude/limits   {"five_hour":{"pct","resets_at"},"seven_day":{"pct","resets_at"},"sampled_at"}
 * All retained; times are UTC epoch seconds. Parsed on the network task, read on the main loop.
 */
#include <Arduino.h>

struct ClaudeData
{
  enum State : uint8_t
  {
    unknown, // Nothing received (bridge never ran)
    idle,
    working,
    waiting, // Needs permission / input
    done
  };

  // tubehax/claude/state
  bool has_state = false;
  State state = unknown;
  char tool[20] = "";
  char model[20] = "";
  char project[24] = "";
  uint32_t since = 0;        // State began
  uint32_t work_started = 0; // Current / last turn
  uint32_t work_ended = 0;   // 0 while the turn runs
  uint8_t sessions = 0;

  // tubehax/claude/metrics
  bool has_metrics = false;
  float ctx_pct = 0;
  uint32_t ctx_size = 0, tokens_in = 0, tokens_out = 0;
  float cost_usd = NAN; // NAN: unknown (only the status line reports cost)
  uint32_t duration_s = 0, sampled_at = 0;
  int32_t lines_added = -1, lines_removed = -1; // -1: unknown

  // tubehax/claude/limits
  bool has_limits = false;
  float five_pct = 0, seven_pct = 0;
  uint32_t five_resets = 0, seven_resets = 0;

  uint32_t version = 0; // Incremented on every received message
};

// MQTT: topic filter to subscribe, and the handler for matching messages (network task).
extern const char *const CLAUDE_TOPICS;
bool ClaudeDataHandle(const char *topic, const uint8_t *payload, size_t length);

// Main loop: copy of the latest data. Cheap; compare .version to detect changes.
void ClaudeDataGet(ClaudeData &out);

#endif // CLAUDE_DATA_H
