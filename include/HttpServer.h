#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

// The clock's web server on port 80 (http://<UniqueDeviceName>.local/): remote control API (Remote.h)
// and OTA uploads (OTA.h). Starts once WiFi is connected; call on every network task loop.
void HttpLoop();

#endif // HTTP_SERVER_H
