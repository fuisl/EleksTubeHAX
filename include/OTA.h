#ifndef OTA_H
#define OTA_H

#include "GLOBAL_DEFINES.h"

#ifdef OTA_ENABLED
// Over-the-air firmware and filesystem updates via HTTP POST /update (see OTA.cpp, scripts/ota.sh).
// Starts the web server once WiFi is connected, then services requests. Call on every loop.
// While an upload is running this call blocks and shows the progress on all six displays.
void OtaLoop();
#endif

#endif // OTA_H
