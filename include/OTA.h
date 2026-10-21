#ifndef OTA_H
#define OTA_H

#include "GLOBAL_DEFINES.h"

#ifdef OTA_ENABLED
class WebServer;

// Over-the-air firmware and filesystem updates via HTTP POST /update (see OTA.cpp, scripts/ota.sh).
// While an upload is running the web server blocks and shows the progress on all six displays.
void OtaRegisterRoutes(WebServer &server);
#endif

#endif // OTA_H
