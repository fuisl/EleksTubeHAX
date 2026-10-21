/*
 * Author: Aljaz Ogrin
 * Project: Alternative firmware for EleksTube IPS clock
 * Original location: https://github.com/aly-fly/EleksTubeHAX
 * Hardware: ESP32
 * Based on: https://github.com/SmittyHalibut/EleksTubeHAX
 */

#include <nvs_flash.h>
#include <stdint.h>
#include <math.h>
#include <Wire.h>
#include <SPI.h>
#include "GLOBAL_DEFINES.h"
#include "Backlights.h"
#include "Buttons.h"
#include "Clock.h"
#include "StoredConfig.h"
#include "TFTs.h"
#include "WiFi_WPS.h"
#include "NetTask.h"
#include "Remote.h"
#include "App.h"
#include "ClockApp.h"
#include "CountdownApp.h"
#include "ClaudeApp.h"
#include "SettingsApp.h"

#ifdef GEOLOCATION_ENABLED
#include "IPGeolocation_AO.h"
#endif

#if defined(MQTT_PLAIN_ENABLED) || defined(MQTT_HOME_ASSISTANT)
#include "MQTT_client_ips.h"
#endif

char UniqueDeviceName[32]; // Enough space for <DeviceName> + 6 hex chars + null

Backlights backlights;
Buttons buttons;
TFTs tfts;
Clock uclock;
StoredConfig stored_config;

AppManager apps;
ClockApp clockApp;
CountdownApp countdownApp;
ClaudeApp claudeApp;
SettingsApp settingsApp(clockApp);
App *const modeCycle[] = {&clockApp, &countdownApp, &claudeApp}; // MODE tap order

#ifdef GEOLOCATION_ENABLED
double GeoLocTZoffset = 0;
bool GetGeoLocationTimeZoneOffset();
constexpr uint8_t GEOLOC_MAX_FAILURES_PER_DAY = 4;
constexpr uint32_t GEOLOC_RETRY_BACKOFF_MS = 5UL * 60UL * 1000UL;
uint8_t GeoLocFailedAttempts = 0;
uint32_t GeoLocNextRetryMillis = 0;
uint8_t GeoLocAttemptDay = 0;
bool GeoLocNeedsUpdate = false;
void processGeoLocUpdate(void);
void checkUpdateGeoLocNeeded(void);
uint8_t yesterday = 0;
#endif

#ifdef DIMMING
bool isDimmingNeeded = false;
uint8_t hour_old = 255;
#endif

uint32_t lastMQTTCommandExecuted = (uint32_t)-1;

// Helper function, defined below.
#ifdef DIMMING
bool isNightTime(uint8_t current_hour);
void checkDimmingNeeded(void);
#endif

//-----------------------------------------------------------------------
// Setup
//-----------------------------------------------------------------------
void setup()
{
  Serial.begin(115200);
  delay(500); // Wait for serial monitor to catch up

  Serial.println("\nSystem starting...\n");
  Serial.println("EleksTubeHAX https://github.com/aly-fly/EleksTubeHAX");
  Serial.printf("Firmware version: v%s.\n", FIRMWARE_VERSION);
  Serial.printf("Reset reason: %d (1 power-on, 3 software, 4 panic, 5/6/7 watchdog, 9 brownout)\n", esp_reset_reason());

  // Prepare unique device name
#ifdef MQTT_CLIENT_ID_FOR_SMARTNEST
  snprintf(UniqueDeviceName, sizeof(UniqueDeviceName), "%s", MQTT_CLIENT_ID_FOR_SMARTNEST); // Use fixed ID for smartnest.cz
#else
  {
    // Generate unique ID for other cases
    // ESP.getEfuseMac() returns the 48-bit MAC address in the lower bits of the value (little-endian byte order when viewed as an integer).
    // We extract all 6 bytes and store them in the canonical MAC order (MSB first).
    uint64_t rawmac = ESP.getEfuseMac() & 0xFFFFFFFFFFFFULL; // Keep only lower 48 bits, comes in reverse order
    uint8_t mac_bytes[6];
    for (int i = 0; i < 6; i++)
    {
      mac_bytes[i] = (rawmac >> (8 * i)) & 0xFF; // LSB first
    }
    // Generate a unique device name using the last 3 bytes of the MAC address to make the name shorter but still unique enough to avoid collisions
    snprintf(UniqueDeviceName, sizeof(UniqueDeviceName), "%s-%02X%02X%02X", DEVICE_NAME,
             mac_bytes[3], mac_bytes[4], mac_bytes[5]);
  }
#endif // #ifdef MQTT_CLIENT_ID_FOR_SMARTNEST
  // Prepare lowercase variant for MQTT topic usage; replace any character that is
  // not alphanumeric or '-' with '_' to keep the name safe for MQTT client IDs and topics.
  for (size_t i = 0; i < sizeof(UniqueDeviceName); ++i)
  {
    char c = UniqueDeviceName[i];
    if (c == '\0')
      break;
    c = (char)tolower((int)c);
    if (!isalnum((unsigned char)c) && c != '-' && c != '_')
      c = '_';
    UniqueDeviceName[i] = c;
  }
  Serial.printf("Set device name: \"%s\".\n", UniqueDeviceName);

  Serial.print("Init NVS flash partition usage...");
  esp_err_t ret = nvs_flash_init(); // Initialize NVS
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    Serial.println("");
    Serial.println("No free pages in or newer version of NVS partition found. Erasing NVS flash partition...");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  Serial.println("Done.");

  stored_config.begin();
  stored_config.load();
  StoredConfig::Config::Display &display = stored_config.config.display;
  if (display.is_valid != StoredConfig::valid)
  { // First boot with TubeHAX settings: defaults from _USER_DEFINES.h
    display = {true, NIGHT_TIME, DAY_TIME, BACKLIGHT_DIMMED_INTENSITY, TFT_DIMMED_INTENSITY, true, StoredConfig::valid};
  }

  backlights.begin(&stored_config.config.backlights);
  backlights.dimmed_intensity = display.night_backlight;
  buttons.begin();

  // Setup the displays (TFTs) initaly and show bootup message(s).
  tfts.begin(); // ...and count number of clock faces available...
  tfts.fillScreen(TFT_BLACK);

  tfts.setTextColor(TFT_WHITE, TFT_BLACK);
  tfts.setCursor(0, 0, 2); // Font 2. 16 pixel high
  tfts.println("Starting Setup...");

  // Setup WiFi connection. Must be done before setting up Clock.
  // This is done outside Clock so the network can be used for other things.
  tfts.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  tfts.println("WiFi start...");
  Serial.println("WiFi start...");
  WifiBegin();
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);

  // Wait a bit (5x100ms = 0.5 sec) before querying NTP.
  for (uint8_t ndx = 0; ndx < 5; ndx++)
  {
    tfts.print(">");
    delay(100);
  }
  tfts.println("");

  // Setup the clock. It needs WiFi to be established already.
  tfts.setTextColor(TFT_MAGENTA, TFT_BLACK);
  tfts.print("Clock start...");
  Serial.println("\nClock start-up...");
  uclock.begin(&stored_config.config.uclock);
  tfts.println("Done!");
  Serial.println("\nClock start-up done!");
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);

#if defined(MQTT_PLAIN_ENABLED) || defined(MQTT_HOME_ASSISTANT)
  // Setup MQTT.
  tfts.setTextColor(TFT_YELLOW, TFT_BLACK);
  tfts.print("MQTT start...");
  Serial.println("\nMQTT start...");
  MQTTStart(false);
  tfts.println("Done!");
  Serial.println("MQTT start Done!");
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);
#endif

#ifdef GEOLOCATION_ENABLED
  tfts.setTextColor(TFT_CYAN, TFT_BLACK);
  tfts.println("GeoLoc query...");
  if (GetGeoLocationTimeZoneOffset())
  {
    tfts.print("TZ: ");
    Serial.print("TZ: ");
    tfts.println(GeoLocTZoffset);
    Serial.println(GeoLocTZoffset);
    uclock.setTimeZoneOffset(GeoLocTZoffset * 3600);
    Serial.println();
    Serial.print("Saving config! Triggered by timezone change...");
    stored_config.save();
    tfts.println("Done!");
    Serial.println("Done!");
    tfts.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  else
  {
    tfts.setTextColor(TFT_RED, TFT_BLACK);
    tfts.println("GeoLoc FAILED");
    Serial.println("GeoLoc failed!");
    tfts.setTextColor(TFT_WHITE, TFT_BLACK);
  }
#endif // #ifdef GEOLOCATION_ENABLED

  if (uclock.getActiveGraphicIdx() > tfts.NumberOfClockFaces)
  {
    uclock.setActiveGraphicIdx(tfts.NumberOfClockFaces);
    Serial.println("Last selected index of clock face is larger than currently available number of image sets.");
  }
  if (uclock.getActiveGraphicIdx() < 1)
  {
    uclock.setActiveGraphicIdx(1);
    Serial.println("Last selected index of clock face is less than 1.");
  }
  tfts.current_graphic = uclock.getActiveGraphicIdx();

  tfts.setTextColor(TFT_WHITE, TFT_BLACK);
  tfts.println("Done with Setup!");
  Serial.println("\nDone with Setup!");

  // Leave bootup messages on screen for a few seconds (10x200ms = 2 sec).
  for (uint8_t ndx = 0; ndx < 10; ndx++)
  {
    tfts.print(">");
    delay(200);
  }

  // Start up the clock displays.
  tfts.fillScreen(TFT_BLACK);
  uclock.loop();
  apps.begin(&clockApp, &settingsApp, modeCycle, sizeof(modeCycle) / sizeof(modeCycle[0])); // Draws the clock
  RemoteBegin();
  NetTaskBegin(); // From here on WiFi, MQTT, NTP, geolocation and OTA run on core 0
  Serial.println("Starting main loop...");
}

//-----------------------------------------------------------------------
// Main loop
//-----------------------------------------------------------------------
void loop()
{
  uint32_t millis_at_top = millis();

  // Networking (WiFi, MQTT, NTP, geolocation, OTA) runs in the network task; see NetTask.h.
  tfts.lock(); // Released before the idle delay below

#if defined(MQTT_PLAIN_ENABLED) || defined(MQTT_HOME_ASSISTANT)
  bool MQTTCommandReceived =
      MQTTCommandMainPowerReceived ||
      MQTTCommandBackPowerReceived ||
      MQTTCommandStateReceived ||
      MQTTCommandBrightnessReceived ||
      MQTTCommandMainBrightnessReceived ||
      MQTTCommandBackBrightnessReceived ||
      MQTTCommandPatternReceived ||
      MQTTCommandBackPatternReceived ||
      MQTTCommandBackColorPhaseReceived ||
      MQTTCommandGraphicReceived ||
      MQTTCommandMainGraphicReceived ||
      MQTTCommandUseTwelveHoursReceived ||
      MQTTCommandBlankZeroHoursReceived ||
      MQTTCommandPulseBpmReceived ||
      MQTTCommandBreathBpmReceived ||
      MQTTCommandRainbowSecReceived;

  if (MQTTCommandMainPowerReceived)
  {
    MQTTCommandMainPowerReceived = false;
    if (MQTTCommandMainPower)
    {
      // Perform reinit, enable, redraw only if displays are actually off. HA sends ON command together with clock face change which causes flickering.
      if (!tfts.isEnabled())
      {
        tfts.enableAllDisplays();
        apps.redraw(); // Needed because the displays were blanked before turning off
      }
    }
    else
    {
      tfts.chip_select.setAll();
      tfts.fillScreen(TFT_BLACK); // Blank the screens before turning off; needed for all clocks without a real power switch circuit to "simulate" the switched-off displays
      tfts.disableAllDisplays();
    }
  }

  if (MQTTCommandBackPowerReceived)
  {
    MQTTCommandBackPowerReceived = false;
    if (MQTTCommandBackPower)
    {
      backlights.PowerOn();
    }
    else
    {
      backlights.PowerOff();
    }
  }

  if (MQTTCommandStateReceived)
  {
    MQTTCommandStateReceived = false;
    randomSeed(millis());
    uint8_t idx;
    if (MQTTCommandState >= 90)
    {
      idx = random(1, tfts.NumberOfClockFaces + 1);
    }
    else
    {
      idx = (MQTTCommandState / 5) - 1;
    } // 10..40 -> graphic 1..6
    Serial.print("Graphic change request from MQTT; command: ");
    Serial.print(MQTTCommandState);
    Serial.print(", index: ");
    Serial.println(idx);
    uclock.setClockGraphicsIdx(idx);
    tfts.current_graphic = uclock.getActiveGraphicIdx();
    apps.redraw(); // Redraw everything
  }

  if (MQTTCommandMainBrightnessReceived)
  {
    MQTTCommandMainBrightnessReceived = false;
    tfts.dimming = MQTTCommandMainBrightness;
    tfts.ProcessUpdatedDimming();
    apps.redraw();
  }

  if (MQTTCommandBackBrightnessReceived)
  {
    MQTTCommandBackBrightnessReceived = false;
    backlights.setIntensity(uint8_t(MQTTCommandBackBrightness));
  }

  if (MQTTCommandPatternReceived)
  {
    MQTTCommandPatternReceived = false;

    for (int8_t i = 0; i < Backlights::num_patterns; i++)
    {
      Serial.print("New pattern ");
      Serial.print(MQTTCommandPattern);
      Serial.print(", check pattern ");
      Serial.println(Backlights::patterns_str[i]);
      if (strcmp(MQTTCommandPattern, (Backlights::patterns_str[i]).c_str()) == 0)
      {
        backlights.setPattern(Backlights::patterns(i));
        break;
      }
    }
  }

  if (MQTTCommandBackPatternReceived)
  {
    MQTTCommandBackPatternReceived = false;
    for (int8_t i = 0; i < Backlights::num_patterns; i++)
    {
      Serial.print("new pattern ");
      Serial.print(MQTTCommandBackPattern);
      Serial.print(", check pattern ");
      Serial.println(Backlights::patterns_str[i]);
      if (strcmp(MQTTCommandBackPattern, (Backlights::patterns_str[i]).c_str()) == 0)
      {
        backlights.setPattern(Backlights::patterns(i));
        break;
      }
    }
  }

  if (MQTTCommandBackColorPhaseReceived)
  {
    MQTTCommandBackColorPhaseReceived = false;

    backlights.setColorPhase(MQTTCommandBackColorPhase);
  }

  if (MQTTCommandGraphicReceived)
  {
    MQTTCommandGraphicReceived = false;

    uclock.setClockGraphicsIdx(MQTTCommandGraphic);
    tfts.current_graphic = uclock.getActiveGraphicIdx();
    apps.redraw(); // Redraw everything
  }

  if (MQTTCommandMainGraphicReceived)
  {
    MQTTCommandMainGraphicReceived = false;
    uclock.setClockGraphicsIdx(MQTTCommandMainGraphic);
    tfts.current_graphic = uclock.getActiveGraphicIdx();
    apps.redraw(); // Redraw everything
  }

  if (MQTTCommandUseTwelveHoursReceived)
  {
    MQTTCommandUseTwelveHoursReceived = false;
    uclock.setTwelveHour(MQTTCommandUseTwelveHours);
  }

  if (MQTTCommandBlankZeroHoursReceived)
  {
    MQTTCommandBlankZeroHoursReceived = false;
    uclock.setBlankHoursZero(MQTTCommandBlankZeroHours);
  }

  if (MQTTCommandPulseBpmReceived)
  {
    MQTTCommandPulseBpmReceived = false;
    backlights.setPulseRate(MQTTCommandPulseBpm);
  }

  if (MQTTCommandBreathBpmReceived)
  {
    MQTTCommandBreathBpmReceived = false;
    backlights.setBreathRate(MQTTCommandBreathBpm);
  }

  if (MQTTCommandRainbowSecReceived)
  {
    MQTTCommandRainbowSecReceived = false;
    backlights.setRainbowDuration(MQTTCommandRainbowSec);
  }

  MQTTStatusMainPower = tfts.isEnabled();
  MQTTStatusBackPower = backlights.getPower();
  MQTTStatusState = (uclock.getActiveGraphicIdx() + 1) * 5; // 10
  MQTTStatusBrightness = backlights.getIntensity();
  MQTTStatusMainBrightness = tfts.dimming;
  MQTTStatusBackBrightness = backlights.getEffectiveIntensity(); // report what the LEDs really show (dimmed level at night)
  strcpy(MQTTStatusPattern, backlights.getPatternStr().c_str());
  strcpy(MQTTStatusBackPattern, backlights.getPatternStr().c_str());
  backlights.getPatternStr().toCharArray(MQTTStatusBackPattern, backlights.getPatternStr().length() + 1);
  MQTTStatusBackColorPhase = backlights.getColorPhase();
  MQTTStatusGraphic = uclock.getActiveGraphicIdx();
  MQTTStatusMainGraphic = uclock.getActiveGraphicIdx();
  MQTTStatusUseTwelveHours = uclock.getTwelveHour();
  MQTTStatusBlankZeroHours = uclock.getBlankHoursZero();
  MQTTStatusPulseBpm = backlights.getPulseRate();
  MQTTStatusBreathBpm = backlights.getBreathRate();
  MQTTStatusRainbowSec = backlights.getRainbowDuration();

  if (MQTTCommandReceived)
  {
    lastMQTTCommandExecuted = millis();
    MQTTReportRequested = true; // Published by the network task
  }

  if (lastMQTTCommandExecuted != -1)
  {
    if (((millis() - lastMQTTCommandExecuted) > (MQTT_SAVE_PREFERENCES_AFTER_SEC * 1000)) && !apps.isActive(&settingsApp))
    { // Save the config after a while (default is 60 seconds) if no new MQTT command was received and we are not in the menu.
      lastMQTTCommandExecuted = -1;

      Serial.print("Saving config...");
      stored_config.save();
      Serial.println(" Done.");
    }
  }
#endif

  buttons.loop();
  uclock.loop();
  apps.loop(buttons); // Button gestures, mode switching, timers and drawing of the active mode
  RemoteLoop();       // Commands from MQTT / web API, state for the admin page
  backlights.loop();  // After the modes, they may override the LEDs

#ifdef DIMMING
  checkDimmingNeeded(); // Night or day time brightness change
#endif


  uint32_t time_in_loop = millis() - millis_at_top;
  if (time_in_loop < 20)
  {
    // we have free time (loop run took under 20 ms), spend it for loading next image into buffer
    tfts.LoadNextImage();

  }
  tfts.unlock();

  // Sleep for up to 20ms, less if we've spent time doing stuff above.
  time_in_loop = millis() - millis_at_top;
  if (time_in_loop < 20)
    delay(20 - time_in_loop);
  time_in_loop = millis() - millis_at_top;
  if (time_in_loop > 250)
    Serial.printf("Slow loop: %lu ms (mode %s, free heap %u)\n", time_in_loop, apps.active()->name(), ESP.getFreeHeap());
#ifdef DEBUG_OUTPUT
  if (time_in_loop <= 2) // if the loop time is less than 2ms, we don't need to print it in detail
    Serial.print(".");
  else
  {
    Serial.print("time spent in loop (ms): "); // print the time spent in the loop
    Serial.println(time_in_loop);
  }
#endif // DEBUG_OUTPUT
}

#ifdef DIMMING
bool isNightTime(uint8_t current_hour)
{ // check the actual hour is in the configured "night time" (settings: admin page / MQTT)
  const StoredConfig::Config::Display &d = stored_config.config.display;
  if (!d.dimming || d.night_start == d.day_start)
    return false;
  if (d.day_start < d.night_start)
  { // "Night" spans across midnight so it is split between two days
    return (current_hour < d.day_start) || (current_hour >= d.night_start);
  }
  else
  { // "Night" starts after midnight, entirely contained within the current day
    return (current_hour >= d.night_start) && (current_hour < d.day_start);
  }
}

// Settings changed at runtime (Remote): re-evaluate night dimming and redraw.
void SettingsChanged()
{
  backlights.dimmed_intensity = stored_config.config.display.night_backlight;
  hour_old = 255;
  checkDimmingNeeded();
  apps.redraw();
}

void checkDimmingNeeded()
{                                             // dim the display in the defined night time
  uint8_t current_hour = uclock.getHour24();  // for internal calcs we always use 24h format
  isDimmingNeeded = current_hour != hour_old; // check, if the hour has changed since last loop (from time passing by or from timezone change)
  if (isDimmingNeeded)
  {
    Serial.print("Current hour = ");
    Serial.print(current_hour);
    Serial.print(", Night Time Start = ");
    Serial.print(stored_config.config.display.night_start);
    Serial.print(", Day Time Start = ");
    Serial.println(stored_config.config.display.day_start);
    if (isNightTime(current_hour))
    { // check if it is in the defined night time
      Serial.println("Set to night time mode (dimmed)!");
      tfts.dimming = stored_config.config.display.night_tft;
      tfts.ProcessUpdatedDimming();
      backlights.setDimming(true);
    }
    else
    {
      Serial.println("Set to day time mode (max brightness)!");
      tfts.dimming = 255; // 0..255
      tfts.ProcessUpdatedDimming();
      backlights.setDimming(false); // restore the configured backlight intensity
    }
    Serial.print("Backlight intensity now: ");
    Serial.println(backlights.getEffectiveIntensity());
    apps.redraw(); // Redraw everything; software dimming will be done here
    hour_old = current_hour;
  }
}
#endif // DIMMING

#ifdef GEOLOCATION_ENABLED
bool GetGeoLocationTimeZoneOffset()
{
  Serial.println("\nStarting Geolocation API query...");

#ifdef GEOLOCATION_PROVIDER_IPAPI
  // Use IP-API.com -> Free tier has a 45 requests per minute limit!
  IPGeolocation location(GEOLOCATION_API_KEY, "IPAPI");
#elif defined(GEOLOCATION_PROVIDER_IPGEOLOCATION)
  // Use ipgeolocation.io -> Free tier has 1,000 requests per month limit!
  IPGeolocation location(GEOLOCATION_API_KEY, "IPGEOLOCATION");
#elif defined(GEOLOCATION_PROVIDER_ABSTRACTAPI)
  // Use AbstractAPI.com -> Free tier has 1,000 requests AT ALL per account!
  IPGeolocation location(GEOLOCATION_API_KEY, "ABSTRACTAPI");
#else
  // No provider defined -> default to IP-API.com
  IPGeolocation location(GEOLOCATION_API_KEY, "IPAPI");
#endif

  IPGeo ipg;
  if (location.updateStatus(&ipg))
  {
    Serial.println(String("Geo Time Zone: ") + String(ipg.tz));
    Serial.println(String("Geo TZ Offset: ") + String(ipg.offset));          // primary value of interest
    Serial.println(String("Geo Current Time: ") + String(ipg.current_time)); // currently unused but handy for debugging
    const double rawOffsetHours = ipg.offset;
    const int32_t newOffsetSeconds = static_cast<int32_t>(lround(rawOffsetHours * 3600.0));

    if ((newOffsetSeconds % (15 * 60)) != 0)
    {
      Serial.print("GeoLoc rejected offset not aligned to 15 min grid (seconds): ");
      Serial.println(newOffsetSeconds);
      return false;
    }

    const bool hasValidStoredOffset = stored_config.config.uclock.is_valid == StoredConfig::valid;
    const int32_t previousOffsetSeconds = static_cast<int32_t>(stored_config.config.uclock.time_zone_offset);
    const int32_t defaultOffsetSeconds = 1 * 3600;

    if (hasValidStoredOffset && previousOffsetSeconds != 0 && previousOffsetSeconds != defaultOffsetSeconds)
    {
      int32_t diff = newOffsetSeconds - previousOffsetSeconds;
      if (diff < 0)
      {
        diff = -diff;
      }

      if (diff > (2 * 3600)) // more than 2 hours difference -> verify with retry
      {
        Serial.print("GeoLoc offset deviates by more than 2h from stored value (prev: ");
        Serial.print(previousOffsetSeconds);
        Serial.print("s, new: ");
        Serial.print(newOffsetSeconds);
        Serial.println("s). Retrying in 30s to verify...");
        delay(30000);

        if (location.updateStatus(&ipg))
        {
          const double rawOffsetHoursRetry = ipg.offset;
          const int32_t retryOffsetSeconds = static_cast<int32_t>(lround(rawOffsetHoursRetry * 3600.0));

          if ((retryOffsetSeconds % (15 * 60)) == 0)
          {
            int32_t diffRetry = retryOffsetSeconds - newOffsetSeconds;
            if (diffRetry < 0) diffRetry = -diffRetry;

            if (diffRetry <= 900)
            {
              Serial.print("GeoLoc retry confirmed new offset (");
              Serial.print(retryOffsetSeconds);
              Serial.println("s). Applying update.");
              GeoLocTZoffset = static_cast<double>(retryOffsetSeconds) / 3600.0;
              return true;
            }

            Serial.print("GeoLoc retry offset (");
            Serial.print(retryOffsetSeconds);
            Serial.print("s) differs from first attempt (");
            Serial.print(newOffsetSeconds);
            Serial.println("s). Update rejected.");
            return false;
          }

          Serial.println("GeoLoc retry returned offset not aligned to 15-min grid. Rejected.");
          return false;
        }

        Serial.println("GeoLoc retry failed. Update rejected.");
        return false;
      }
    }

    GeoLocTZoffset = static_cast<double>(newOffsetSeconds) / 3600.0;
    Serial.println(String("Geo TZ Offset (applied): ") + String(GeoLocTZoffset));
    return true;
  }

  Serial.println("Geolocation failed.");
  return false;
}
#endif

#ifdef GEOLOCATION_ENABLED
void checkUpdateGeoLocNeeded()
{
  uint8_t currentDay = uclock.getDay(); // Get current day of month
  const uint8_t currentWeekday = weekday(uclock.loop_time);
  const bool isSunday = (currentWeekday == 1); // TimeLib defines Sunday as weekday 1

  // Only on Sundays, and only if the day has changed since last successful update
  // `isGeoLocWindow` is set to true between 03:00:05 and 03:00:59, because daylight saving time changes usually happen at 03:00 local time.
  // GeoLoc update mechanism in the main 'loop free time slot' is then triggered, because `GeoLocNeedsUpdate` is set to true here.
  const bool isGeoLocWindow = isSunday && (currentDay != yesterday) && (uclock.getHour24() == 3) && (uclock.getMinute() == 0) && (uclock.getSecond() > 5);

  if (!GeoLocNeedsUpdate && isGeoLocWindow)
  {
    Serial.print("GeoLoc needs update! Current date (DD.MM.YYYY): ");
    Serial.print(currentDay);
    Serial.print(".");
    Serial.print(uclock.getMonth());
    Serial.print(".");
    Serial.println(uclock.getYear());

    // Set flags and counters for GeoLoc update process
    GeoLocNeedsUpdate = true;
    GeoLocFailedAttempts = 0;
    GeoLocNextRetryMillis = 0;
    GeoLocAttemptDay = currentDay;
  }
}

void processGeoLocUpdate()
{
  if (!GeoLocNeedsUpdate)
  {
    return; // no update needed
  }

  const uint32_t now = millis();
  const uint8_t today = uclock.getDay();

  if (GeoLocAttemptDay != today)
  { // New day, reset attempt counter
    GeoLocFailedAttempts = 0;
    GeoLocNextRetryMillis = 0;
    GeoLocAttemptDay = today;
  }

  if (GeoLocFailedAttempts >= GEOLOC_MAX_FAILURES_PER_DAY)
  {
    Serial.println("GeoLocation update skipped: failure limit reached for today.");
    GeoLocNeedsUpdate = false;
    return; // give up for today
  }

  if (now < GeoLocNextRetryMillis)
  {
    return; // not yet time for next retry
  }

  Serial.println("Daily update for geolocation timezone offset...");

  const int32_t GeoLocTZOffsetOld = uclock.getTimeZoneOffset() / 3600;
  Serial.print("Current TZ offset (hours): ");
  Serial.println(GeoLocTZOffsetOld);

  Serial.println("Querying GeoLocation API...");
  if (GetGeoLocationTimeZoneOffset())
  {
    const int32_t GeoLocTOffsetNew = uclock.getTimeZoneOffset() / 3600;
    Serial.print("New TZ offset (hours): ");
    Serial.println(GeoLocTOffsetNew);

    GeoLocNeedsUpdate = false;
    GeoLocFailedAttempts = 0;
    GeoLocNextRetryMillis = 0;
    yesterday = today;
    return; // success for today!
  }

  GeoLocFailedAttempts++;
  if (GeoLocFailedAttempts >= GEOLOC_MAX_FAILURES_PER_DAY)
  {
    Serial.println("GeoLocation update aborted after repeated failures today.");
    GeoLocNeedsUpdate = false;
    return; // give up for today
  }

  // Schedule next retry
  GeoLocNextRetryMillis = now + GEOLOC_RETRY_BACKOFF_MS;
  Serial.print("GeoLocation retry scheduled in ");
  Serial.print(GEOLOC_RETRY_BACKOFF_MS / 1000);
  Serial.println(" seconds.");
}
#endif // GEOLOCATION_ENABLED
