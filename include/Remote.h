#ifndef REMOTE_H
#define REMOTE_H

/*
 * Remote control of the clock: JSON commands and a JSON state snapshot, shared by
 *   MQTT      tubehax/clock/<id>/cmd  (in)   tubehax/clock/<id>/state  (out, retained)
 *   HTTP      POST /api/cmd, GET /api/state, GET /api/countdown?..., GET /api/mode?name=...
 * used by the admin page on the homelab and the iPhone shortcut (docs/DESIGN.md#remote-control).
 *
 * Commands, e.g. {"mode":"countdown"}, {"countdown":{"start":1500}}, {"countdown":{"action":"pause"}},
 * {"displays":false}, {"backlights":true}, arrive on the network task and are queued; the main loop
 * applies them (RemoteLoop) and renders the state, which the network task publishes.
 */
#include <Arduino.h>

class WebServer;

void RemoteBegin();

// Network task: parse and queue a command. Returns false (and why) when nothing valid was found.
bool RemoteSubmit(const char *json, size_t length, String &error);

// Main loop: apply queued commands and refresh the state snapshot.
void RemoteLoop();

// Network task: copy of the latest state JSON. Returns true once per change (or heartbeat) to publish.
bool RemoteTakeState(String &json);
String RemoteState();

// Network task: /api/* routes on the clock's web server.
void RemoteRegisterRoutes(WebServer &server);

#endif // REMOTE_H
