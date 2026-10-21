#include "NetTask.h"
#include "GLOBAL_DEFINES.h"
#include "Clock.h"
#include "OTA.h"
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
#ifdef OTA_ENABLED
    OtaLoop();
#endif
#if defined(MQTT_PLAIN_ENABLED) || defined(MQTT_HOME_ASSISTANT)
    MQTTLoopFrequently();
    if (MQTTReportRequested)
    {
      MQTTReportRequested = false;
      MQTTReportBackEverything(true);
    }
    MQTTLoopInFreeTime();
#endif
    uclock.ntpLoop();
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
