#include "HttpServer.h"
#include <ESPmDNS.h>
#include <WebServer.h>
#include "GLOBAL_DEFINES.h"
#include "OTA.h"
#include "Remote.h"
#include "WiFi_WPS.h"

extern char UniqueDeviceName[32];

static WebServer server(80);
static bool listening = false;

void HttpLoop()
{
  if (!listening)
  {
    if (WifiState != connected)
      return;

    MDNS.begin(UniqueDeviceName); // <UniqueDeviceName>.local
    MDNS.addService("http", "tcp", 80);
    RemoteRegisterRoutes(server);
#ifdef OTA_ENABLED
    OtaRegisterRoutes(server);
#endif
    server.onNotFound([]()
                      { server.send(404, "text/plain", "Not found. Try /api/state\n"); });
    server.begin();
    listening = true;
    Serial.printf("HTTP: listening on http://%s.local/ (api/state, api/cmd, update)\n", UniqueDeviceName);
  }
  server.handleClient();
}
