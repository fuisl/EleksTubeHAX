#include "Remote.h"
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <WebServer.h>
#include <WiFi.h>
#include "GLOBAL_DEFINES.h"
#include "App.h"
#include "Backlights.h"
#include "CountdownApp.h"

extern CountdownApp countdownApp; // main.cpp

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
      Backlights
    } kind;
    uint32_t value;
    char name[16];
  };

  enum Action : uint32_t
  {
    ActionStart, // with the set duration
    ActionPause,
    ActionResume,
    ActionToggle, // stopped -> start, running <-> paused
    ActionReset
  };

  const uint32_t STATE_REFRESH_MS = 1000;    // "remaining" in GET /api/state is at most this old
  const uint32_t STATE_HEARTBEAT_MS = 30000; // MQTT publish without a change (RSSI, uptime, remaining)

  QueueHandle_t queue = nullptr;
  portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;
  char state_json[512] = "{}";
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
