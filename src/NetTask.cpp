#include "NetTask.h"
#include "GLOBAL_DEFINES.h"
#include "Clock.h"
#include "HttpServer.h"
#include "Remote.h"
#include "WiFi_WPS.h"

#if defined(MQTT_PLAIN_ENABLED) || defined(MQTT_HOME_ASSISTANT)
#include "MQTT_client_ips.h"
#endif

#ifdef GEOLOCATION_ENABLED
void checkUpdateGeoLocNeeded(); // main.cpp
void processGeoLocUpdate();     // main.cpp
#endif

volatile bool MQTTReportRequested = false;

static void netTask(void *)
{
  for (;;)
  {
    WifiReconnect();
    HttpLoop();
#if defined(MQTT_PLAIN_ENABLED) || defined(MQTT_HOME_ASSISTANT)
    MQTTLoopFrequently();
    if (MQTTReportRequested)
    {
      MQTTReportRequested = false;
      MQTTReportBackEverything(true);
    }
    MQTTPublishRemoteState();
    MQTTLoopInFreeTime();
#endif
    uclock.ntpLoop(); // Can wait ~1 s for an answer; a separate task for it costs heap the countdown needs
#ifdef GEOLOCATION_ENABLED
    checkUpdateGeoLocNeeded();
    processGeoLocUpdate();
#endif
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void NetTaskBegin()
{
  // Core 0 next to the WiFi stack; the Arduino loop (displays) runs on core 1.
  xTaskCreatePinnedToCore(netTask, "net", 12288, nullptr, 1, nullptr, 0);
}
