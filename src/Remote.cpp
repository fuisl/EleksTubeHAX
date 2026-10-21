#include "Remote.h"
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <WebServer.h>
#include <WiFi.h>
#include "GLOBAL_DEFINES.h"
#include "App.h"
#include "Backlights.h"
#include "CountdownApp.h"
#include "Clock.h"
#include "StoredConfig.h"

extern CountdownApp countdownApp; // main.cpp
extern StoredConfig stored_config;
void SettingsChanged(); // main.cpp: re-evaluate night dimming, redraw

namespace
{
  struct Command
  {
    enum Kind : uint8_t
    {
      Mode,
      CountdownStart, // value: duration in ms
      CountdownSet,   // value: duration in ms
      CountdownAction,
      Displays,
      Backlights,
      Setting // name: key, value: int32
    } kind;
    uint32_t value;
    char name[24]; // Mode name or setting key
  };

  enum Action : uint32_t
  {
    ActionStart, // with the set duration
    ActionPause,
    ActionResume,
    ActionToggle, // stopped -> start, running <-> paused
    ActionReset
  };

  // Settings that can be changed remotely: key, min, max (docs/DESIGN.md#mqtt-contract-v1-clock).
  struct SettingSpec
  {
    const char *key;
    int32_t min, max;
  };
  const SettingSpec SETTINGS[] = {
      {"twelve_hour", 0, 1},
      {"blank_hours_zero", 0, 1},
      {"utc_offset_min", -720, 840},
      {"dimming", 0, 1},
      {"night_start", 0, 23},
      {"day_start", 0, 23},
      {"night_backlight", 0, 7},
      {"night_tft", 0, 255},
      {"backlight_intensity", 0, 7},
      {"backlight_hue", 0, 359},
      {"auto_claude", 0, 1},
  };
  const uint32_t SAVE_DELAY_MS = 2000; // Several settings in a row cause one flash write
  uint32_t save_due = 0;

  const uint32_t STATE_REFRESH_MS = 1000;    // "remaining" in GET /api/state is at most this old
  const uint32_t STATE_HEARTBEAT_MS = 30000; // MQTT publish without a change (RSSI, uptime, remaining)

  QueueHandle_t queue = nullptr;
  portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;
  char state_json[1024] = "{}";
  bool state_dirty = false; // Changed since the network task last published

  uint32_t last_refresh = 0, last_publish = 0;

  bool queueCommand(const Command &command)
  {
    return queue != nullptr && xQueueSend(queue, &command, 0) == pdTRUE;
  }

  uint32_t secondsToMs(JsonVariantConst seconds)
  {
    float s = seconds.as<float>();
    return s <= 0 ? 0 : s >= 86400 ? 86400000UL : uint32_t(s * 1000.0f + 0.5f);
  }

  bool parseAction(const char *name, Action &action)
  {
    static const char *const names[] = {"start", "pause", "resume", "toggle", "reset"};
    for (uint8_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
      if (name != nullptr && strcmp(name, names[i]) == 0)
      {
        action = Action(i);
        return true;
      }
    return false;
  }

  void applySetting(const char *key, int32_t v)
  {
    StoredConfig::Config &c = stored_config.config;
    if (strcmp(key, "twelve_hour") == 0)
      c.uclock.twelve_hour = v;
    else if (strcmp(key, "blank_hours_zero") == 0)
      c.uclock.blank_hours_zero = v;
    else if (strcmp(key, "utc_offset_min") == 0)
      uclock.setTimeZoneOffset(time_t(v) * 60);
    else if (strcmp(key, "dimming") == 0)
      c.display.dimming = v;
    else if (strcmp(key, "night_start") == 0)
      c.display.night_start = v;
    else if (strcmp(key, "day_start") == 0)
      c.display.day_start = v;
    else if (strcmp(key, "night_backlight") == 0)
      c.display.night_backlight = v;
    else if (strcmp(key, "night_tft") == 0)
      c.display.night_tft = v;
    else if (strcmp(key, "backlight_intensity") == 0)
      backlights.setIntensity(v);
    else if (strcmp(key, "backlight_hue") == 0)
      backlights.setColorPhase(backlights.hueToPhase(float(v)));
    else if (strcmp(key, "auto_claude") == 0)
      c.display.auto_claude = v;
    save_due = millis() + SAVE_DELAY_MS;
    SettingsChanged();
  }

  void apply(const Command &command)
  {
    switch (command.kind)
    {
    case Command::Mode:
      apps.switchTo(apps.byName(command.name));
      break;
    case Command::CountdownStart:
      countdownApp.start(command.value);
      break;
    case Command::CountdownSet:
      countdownApp.setDuration(command.value);
      break;
    case Command::CountdownAction:
      if (command.value == ActionPause)
        countdownApp.pause();
      else if (command.value == ActionResume || command.value == ActionStart)
        countdownApp.resume();
      else if (command.value == ActionToggle)
      {
        if (strcmp(countdownApp.stateName(), "running") == 0)
          countdownApp.pause();
        else
          countdownApp.resume();
      }
      else if (command.value == ActionReset)
        countdownApp.reset();
      if (command.value != ActionReset && command.value != ActionPause)
        apps.switchTo(&countdownApp);
      break;
    case Command::Displays:
      apps.setDisplaysOn(command.value);
      break;
    case Command::Backlights:
      if (bool(command.value) != backlights.getPower())
        backlights.togglePower();
      break;
    case Command::Setting:
      applySetting(command.name, int32_t(command.value));
      break;
    }
  }

  // Fields that, when changed, are published right away. "remaining" only counts while it is frozen.
  struct StateKey
  {
    App *mode;
    const char *countdown;
    uint32_t set_ms, frozen_ms;
    bool displays, backlights;
    bool operator!=(const StateKey &o) const
    {
      return mode != o.mode || countdown != o.countdown || set_ms != o.set_ms || frozen_ms != o.frozen_ms ||
             displays != o.displays || backlights != o.backlights;
    }
  };

  StateKey stateKey()
  {
    const char *cd = countdownApp.stateName(); // Points to a literal: compared by address
    bool ticking = strcmp(cd, "running") == 0;
    return StateKey{apps.active(), cd, countdownApp.durationMs(), ticking ? 0 : countdownApp.remaining(),
                    apps.displaysOn(), backlights.getPower()};
  }

  void renderState()
  {
    JsonDocument doc;
    doc["v"] = 1;
    doc["mode"] = apps.active() ? apps.active()->name() : "";
    doc["displays"] = apps.displaysOn();
    doc["backlights"] = backlights.getPower();
    JsonObject cd = doc["countdown"].to<JsonObject>();
    const char *cd_state = countdownApp.stateName();
    uint32_t left = countdownApp.remaining();
    cd["state"] = cd_state;
    cd["set"] = countdownApp.durationMs() / 1000.0f;
    cd["remaining"] = left / 1000.0f;
    if (strcmp(cd_state, "running") == 0 && timeStatus() != timeNotSet)
      cd["ends_at"] = uint32_t(now() + (left + 500) / 1000); // UTC epoch, for retained copies read later
    const StoredConfig::Config &c = stored_config.config;
    JsonObject settings = doc["settings"].to<JsonObject>();
    settings["twelve_hour"] = c.uclock.twelve_hour;
    settings["blank_hours_zero"] = c.uclock.blank_hours_zero;
    settings["utc_offset_min"] = int32_t(uclock.getTimeZoneOffset() / 60);
    settings["dimming"] = c.display.dimming;
    settings["night_start"] = c.display.night_start;
    settings["day_start"] = c.display.day_start;
    settings["night_backlight"] = c.display.night_backlight;
    settings["night_tft"] = c.display.night_tft;
    settings["backlight_intensity"] = c.backlights.intensity;
    settings["backlight_rgb"] = backlights.getColor();
    settings["auto_claude"] = c.display.auto_claude;
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    doc["ch"] = WiFi.channel();
    doc["fw"] = FIRMWARE_VERSION;
    doc["uptime"] = millis() / 1000;
    doc["heap"] = ESP.getFreeHeap();

    char buffer[sizeof(state_json)];
    size_t n = serializeJson(doc, buffer, sizeof(buffer));
    if (n == 0 || n >= sizeof(buffer))
      return;
    portENTER_CRITICAL(&state_lock);
    memcpy(state_json, buffer, n + 1);
    portEXIT_CRITICAL(&state_lock);
  }

  void sendJson(WebServer &server, int code, const String &body)
  {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(code, "application/json", body + "\n");
  }

  // Queues the command and answers. The command runs on the main loop within ~20 ms.
  void submitAndReply(WebServer &server, const String &json)
  {
    String error;
    if (RemoteSubmit(json.c_str(), json.length(), error))
      sendJson(server, 200, "{\"ok\":true}");
    else
      sendJson(server, 400, "{\"ok\":false,\"error\":\"" + error + "\"}");
  }
} // namespace

void RemoteBegin()
{
  queue = xQueueCreate(8, sizeof(Command));
}

bool RemoteSubmit(const char *json, size_t length, String &error)
{
  JsonDocument doc;
  DeserializationError parse_error = deserializeJson(doc, json, length);
  if (parse_error)
  {
    error = String("invalid JSON: ") + parse_error.c_str();
    return false;
  }

  uint8_t queued = 0;
  bool full = false;
  auto add = [&](const Command &command)
  {
    if (queueCommand(command))
      queued++;
    else
      full = true;
  };

  if (doc["mode"].is<const char *>())
  {
    Command command{Command::Mode, 0, ""};
    strlcpy(command.name, doc["mode"], sizeof(command.name));
    if (apps.byName(command.name) == nullptr)
    {
      error = "unknown mode";
      return false;
    }
    add(command);
  }
  JsonVariantConst cd = doc["countdown"];
  if (cd["set"].is<float>())
    add(Command{Command::CountdownSet, secondsToMs(cd["set"]), ""});
  if (cd["start"].is<float>())
    add(Command{Command::CountdownStart, secondsToMs(cd["start"]), ""});
  else if (cd["start"].is<bool>() && cd["start"].as<bool>())
    add(Command{Command::CountdownAction, ActionStart, ""});
  Action action;
  if (cd["action"].is<const char *>())
  {
    if (parseAction(cd["action"], action))
      add(Command{Command::CountdownAction, action, ""});
    else
    {
      error = "unknown countdown action";
      return false;
    }
  }
  JsonObjectConst settings = doc["settings"];
  for (JsonPairConst kv : settings)
  {
    const SettingSpec *spec = nullptr;
    for (const SettingSpec &s : SETTINGS)
      if (strcmp(kv.key().c_str(), s.key) == 0)
        spec = &s;
    bool numeric = kv.value().is<int32_t>() || kv.value().is<bool>();
    int32_t v = kv.value().is<bool>() ? int32_t(kv.value().as<bool>()) : kv.value().as<int32_t>();
    if (spec == nullptr || !numeric || v < spec->min || v > spec->max)
    {
      error = String("bad setting: ") + kv.key().c_str();
      return false;
    }
    Command command{Command::Setting, uint32_t(v), ""};
    strlcpy(command.name, spec->key, sizeof(command.name));
    add(command);
  }
  if (doc["displays"].is<bool>())
    add(Command{Command::Displays, doc["displays"].as<bool>(), ""});
  if (doc["backlights"].is<bool>())
    add(Command{Command::Backlights, doc["backlights"].as<bool>(), ""});

  if (full)
    error = "busy, try again";
  else if (queued == 0)
    error = "no known command";
  return queued > 0 && !full;
}

void RemoteLoop()
{
  Command command;
  bool applied = false;
  while (queue != nullptr && xQueueReceive(queue, &command, 0) == pdTRUE)
  {
    Serial.printf("Remote: command %u (%lu %s)\n", command.kind, command.value, command.name);
    apply(command);
    applied = true;
  }

  uint32_t now_ms = millis();
  if (save_due != 0 && int32_t(now_ms - save_due) >= 0)
  {
    save_due = 0;
    stored_config.save();
    Serial.println("Remote: settings saved");
  }
  static StateKey last_key{};
  StateKey key = stateKey();
  bool changed = applied || key != last_key;
  if (changed || now_ms - last_refresh >= STATE_REFRESH_MS)
  {
    renderState();
    last_refresh = now_ms;
    if (changed || now_ms - last_publish >= STATE_HEARTBEAT_MS)
    {
      last_key = key;
      last_publish = now_ms;
      portENTER_CRITICAL(&state_lock);
      state_dirty = true;
      portEXIT_CRITICAL(&state_lock);
    }
  }
}

String RemoteState()
{
  char copy[sizeof(state_json)];
  portENTER_CRITICAL(&state_lock);
  memcpy(copy, state_json, sizeof(copy));
  portEXIT_CRITICAL(&state_lock);
  return String(copy);
}

bool RemoteTakeState(String &json)
{
  portENTER_CRITICAL(&state_lock);
  bool dirty = state_dirty;
  state_dirty = false;
  portEXIT_CRITICAL(&state_lock);
  if (dirty)
    json = RemoteState();
  return dirty;
}

void RemoteRegisterRoutes(WebServer &server)
{
  server.on("/api/state", HTTP_GET, [&server]()
            { sendJson(server, 200, RemoteState()); });

  server.on("/api/cmd", HTTP_POST, [&server]()
            { submitAndReply(server, server.arg("plain")); });

  // Shortcut-friendly: /api/countdown?minutes=25 | ?seconds=90 | ?action=pause|resume|toggle|reset
  server.on("/api/countdown", [&server]()
            {
    JsonDocument doc;
    if (server.hasArg("minutes"))
      doc["countdown"]["start"] = server.arg("minutes").toFloat() * 60.0f;
    else if (server.hasArg("seconds"))
      doc["countdown"]["start"] = server.arg("seconds").toFloat();
    if (server.hasArg("action"))
      doc["countdown"]["action"] = server.arg("action");
    String json;
    serializeJson(doc, json);
    submitAndReply(server, json); });

  // /api/mode?name=clock|countdown
  server.on("/api/mode", [&server]()
            {
    JsonDocument doc;
    doc["mode"] = server.arg("name");
    String json;
    serializeJson(doc, json);
    submitAndReply(server, json); });
}
