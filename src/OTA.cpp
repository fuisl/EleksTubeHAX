#include "OTA.h"

#ifdef OTA_ENABLED
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <Update.h>
#include <WebServer.h>
#include "TFTs.h"
#include "WiFi_WPS.h"

// HTTP upload instead of ArduinoOTA/espota: espota aborts when a chunk takes over ~1 s,
// which happens regularly on a congested 2.4 GHz network. Plain TCP rides out latency spikes.
//
//   POST /update[?size=<bytes>]   multipart form, field "firmware" or "filesystem", HTTP basic auth ota:<OTA_PASSWORD>
//   e.g. curl -u ota:<pw> -F firmware=@EleksTube_Gen2_v1.3.13.bin http://elekstubeg2-xxxxxx.local/update
//   (scripts/ota.sh builds and uploads in one go)

extern char UniqueDeviceName[32];

static WebServer server(80);
static bool otaListening = false;
static bool uploadAuthenticated = false;
static String updateError;
static uint8_t lastPercentShown = 255;

static uint16_t claudeOrange() { return tfts.color565(0xD9, 0x77, 0x57); }

// Draws the same frame on all six displays at once (all chip selects active).
static void showOtaScreen(const char *title)
{
  tfts.enableAllDisplays();
  tfts.chip_select.setAll();
  tfts.fillScreen(TFT_BLACK);
  tfts.setTextColor(claudeOrange(), TFT_BLACK);
  tfts.setCursor(5, 60, 4); // Font 4. 26 pixel high
  tfts.print(title);
  tfts.drawRect(5, 120, TFT_WIDTH - 10, 20, claudeOrange());
}

static void showOtaProgress(uint8_t percent)
{
  if (percent == lastPercentShown)
    return;
  lastPercentShown = percent;
  tfts.chip_select.setAll();
  tfts.fillRect(7, 122, (TFT_WIDTH - 14) * percent / 100, 16, claudeOrange());
  tfts.setTextColor(claudeOrange(), TFT_BLACK);
  tfts.setCursor(5, 160, 4);
  tfts.printf("%3u%%", percent);
}

static void handleUploadChunk()
{
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START)
  {
    updateError = "";
    uploadAuthenticated = server.authenticate("ota", OTA_PASSWORD);
    if (!uploadAuthenticated)
      return;

    bool isFirmware = upload.name != "filesystem";
    Serial.printf("OTA: receiving %s (%s), WiFi RSSI %d dBm\n", isFirmware ? "firmware" : "filesystem", upload.filename.c_str(), WiFi.RSSI());
    if (!isFirmware)
      LittleFS.end(); // Filesystem is about to be overwritten
    lastPercentShown = 255;
    showOtaScreen(isFirmware ? "Firmware" : "Files");
    // ?size=<bytes> (sent by scripts/ota.sh) gives a correct progress bar and lets Update reject oversized images.
    size_t size = server.hasArg("size") ? server.arg("size").toInt() : UPDATE_SIZE_UNKNOWN;
    if (!Update.begin(size, isFirmware ? U_FLASH : U_SPIFFS))
      updateError = Update.errorString();
  }
  else if (!uploadAuthenticated || updateError.length())
  {
    return;
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
      updateError = Update.errorString();
    else if (Update.size())
    {
      uint8_t percent = uint8_t(uint64_t(Update.progress()) * 100 / Update.size());
      if (percent / 10 != lastPercentShown / 10)
        Serial.printf("OTA: %3u%% at %lu ms, RSSI %d dBm\n", percent, millis(), WiFi.RSSI());
      showOtaProgress(percent);
    }
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (!Update.end(true)) // true: accept the received size even if none was announced
      updateError = Update.errorString();
  }
  else if (upload.status == UPLOAD_FILE_ABORTED)
  {
    // The connection dropped mid-upload; the completion handler will never run.
    Update.abort();
    Serial.println("OTA: upload aborted (connection lost), rebooting into the current firmware.");
    delay(500);
    ESP.restart();
  }
}

static void handleUploadDone()
{
  if (!uploadAuthenticated)
    return server.requestAuthentication();

  if (updateError.length())
  {
    Serial.printf("OTA: failed: %s\n", updateError.c_str());
    server.send(500, "text/plain", "Update failed: " + updateError + "\n");
  }
  else
  {
    Serial.println("OTA: done, rebooting.");
    showOtaProgress(100);
    server.send(200, "text/plain", "OK, rebooting\n");
  }
  // On failure the running firmware is untouched; a restart is the simplest way to restore the displays.
  delay(500);
  ESP.restart();
}

void OtaLoop()
{
  if (!otaListening)
  {
    if (WifiState != connected)
      return;

    // Modem sleep adds latency spikes of several hundred ms; the clock is mains powered,
    // so keep the radio awake (also lowers MQTT latency).
    WiFi.setSleep(false);

    MDNS.begin(UniqueDeviceName); // <UniqueDeviceName>.local
    MDNS.addService("http", "tcp", 80);
    server.on("/update", HTTP_POST, handleUploadDone, handleUploadChunk);
    server.begin();
    otaListening = true;
    Serial.printf("OTA: listening on http://%s.local/update\n", UniqueDeviceName);
  }
  server.handleClient();
}
#endif // OTA_ENABLED
