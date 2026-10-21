#ifndef NET_TASK_H
#define NET_TASK_H

/*
 * Everything that can block on the network runs in its own task on core 0: WiFi reconnects, MQTT
 * (connect, publish, receive), the web server (remote API, OTA), NTP queries and geolocation.
 * Not split further: a second task's stack (~9 KB) pushed the countdown's hundredths cache into a
 * heap layout where drawing slowed from 40 to 1 fps. The main loop on
 * core 1 (buttons, modes, displays, backlights) never waits for the network, so a flaky WiFi link
 * can't freeze the tubes.
 *
 * Communication with the main loop stays flag based (MQTT command/status variables, Clock NTP hand-off).
 */
#include <Arduino.h>

void NetTaskBegin();

// Main loop -> network task: publish the complete state to MQTT (after a command was executed).
extern volatile bool MQTTReportRequested;

#endif // NET_TASK_H
