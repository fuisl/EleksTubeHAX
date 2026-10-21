#include "Clock.h"
#include "WiFi_WPS.h"

//-----------------------------------------------------------------------------------------------
// begin RTC chip stuff
//-----------------------------------------------------------------------------------------------

#include <RTClib.h>

RTC_DS3231 RTC; // DS3231, works also with DS1307 or PCF8523

void RtcBegin()
{
  Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN);
  if (!RTC.begin())
  {
    Serial.println("No supported RTC found!");
  }
#ifdef DEBUG_OUTPUT_RTC
  else
  {
    bool RegReadSuccess = false;
    unsigned int ctrl = 0;

    Serial.println("DEBUG_OUTPUT_RTC: DS3231/DS1307 RTC found!");
    Serial.printf("Square Wave output status: 0x%x\r\n", RTC.readSqwPinMode());
    Serial.printf("32KHz output status: %s\r\n", RTC.isEnabled32K() ? "Enabled" : "Disabled");
    // Manually read the control and status registers of the DS3231.
    Wire.beginTransmission(0x68);
    Wire.write(0x0E); // Address of the control register
    Wire.endTransmission();
    RegReadSuccess = Wire.requestFrom(0x68, 2);
    Serial.printf("Read from control and status registers: %s\r\n", RegReadSuccess ? "Success" : "Failed!");
    if (RegReadSuccess)
    {
      ctrl = Wire.read();
      Serial.println("DS3231 Control Register:");
      Serial.printf("EOSC:%d BBSQW:%d CONV:%d RS2:%d RS1:%d INTCN:%d A2IE:%d A1IE:%d\r\n",
                    (ctrl & 0x80) >> 7, (ctrl & 0x40) >> 6, (ctrl & 0x20) >> 5, (ctrl & 0x10) >> 4, (ctrl & 0x08) >> 3,
                    (ctrl & 0x04) >> 2, (ctrl & 0x02) >> 1, (ctrl & 0x01));

      ctrl = ctrl >> 8;
      Serial.println("DS3231 Status Register:");
      Serial.printf("OSF:%d EN32Khz:%d BSY:%d A2F:%d A1F:%d\r\n",
                    (ctrl & 0x80) >> 7, (ctrl & 0x08) >> 3, (ctrl & 0x04) >> 2,
                    (ctrl & 0x02) >> 1, (ctrl & 0x01));
    }
    Serial.println("Forcing temperature conversion now.");
    Wire.beginTransmission(0x68);
    Wire.write(0x0E);
    Wire.write(0x3C); // Set CONV=1, set RS2,RS1,INTCN = 1
    Wire.endTransmission();
    delay(5); // Allow some time for BSY to be set

    Wire.beginTransmission(0x68);
    Wire.write(0x0F);
    Wire.endTransmission();
    RegReadSuccess = Wire.requestFrom(0x68, 1);
    if (RegReadSuccess)
    {
      ctrl = 0;
      ctrl = Wire.read();
      Serial.printf("Temperature conversion busy flag: %s\r\n", ((ctrl & 0x04) >> 2) ? "Set!" : "Not set..");
      Serial.println("Waiting 2 seconds for temperature conversion to finish.");
      delay(2000);
      Serial.printf("DS3231 Temperature: %f C\r\n", RTC.getTemperature());
    }
    else
    {
      Serial.println("Unable to read from DS3231!");
    }
  }
#endif

  // Check if the RTC chip reports a power failure.
  bool bPowerLost = 0;
  bPowerLost = RTC.lostPower();
  if (bPowerLost)
  {
    Serial.println("DS3231/DS1307 RTC reports power was lost! Setting time to default value.");
    RTC.adjust(DateTime(2023, 1, 1, 0, 0, 0)); // Set the RTC time to a default value
  }
  else
  {
#ifdef DEBUG_OUTPUT_RTC
    Serial.println("DEBUG_OUTPUT_RTC: DS3231/DS1307 RTC power is OK!");
#endif
  }
}

uint32_t RtcGet()
{
  DateTime now = RTC.now(); // Convert to Unix time
  uint32_t returnvalue = now.unixtime();
#ifdef DEBUG_OUTPUT_RTC
  Serial.print("DEBUG_OUTPUT_RTC: DS3231/DS1307 RTC now.unixtime() returned: ");
  Serial.println(returnvalue);
#endif
  return returnvalue;
}

void RtcSet(uint32_t tt)
{
#ifdef DEBUG_OUTPUT_RTC
  Serial.print("DEBUG_OUTPUT_RTC: Attempting to set DS3231/DS1307 RTC to: ");
  Serial.println(tt);
#endif

  DateTime timetoset(tt); // Convert to Unix time
  RTC.adjust(timetoset);  // Set the RTC time
#ifdef DEBUG_OUTPUT
  Serial.println("DEBUG_OUTPUT_RTC: DS3231/DS1307 RTC time updated.");
#endif
}

//-----------------------------------------------------------------------------------------------
// end of RTC chip stuff
//-----------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------------
// clock class stuff
//-----------------------------------------------------------------------------------------------

// Global variables for clock/NTP client
uint32_t Clock::millis_last_ntp = 0;
volatile bool Clock::ntp_pending = false;
volatile time_t Clock::ntp_pending_time = 0;
volatile uint32_t Clock::ntp_pending_millis = 0;
WiFiUDP Clock::ntpUDP;
NTPClient Clock::ntpTimeClient(ntpUDP, NTP_SERVER, 0, NTP_UPDATE_INTERVAL);

// Adaptive NTP sync variables
uint32_t Clock::current_ntp_interval_ms = Clock::ntp_interval_initial_ms;
uint8_t Clock::consecutive_failures = 0;
uint8_t Clock::consecutive_successes = 0;

void Clock::begin(StoredConfig::Config::Clock *config_)
{
  config = config_;

  if (config->is_valid != StoredConfig::valid)
  {
    // Config is invalid, probably a new device never had its config written.
    // Load some reasonable defaults.
    Serial.println("Loaded Clock config is invalid, using default config values. This is normal on first boot.");
    setTwelveHour(false);
    setBlankHoursZero(false);
    setTimeZoneOffset(1 * 3600); // CET
    setActiveGraphicIdx(1);
    config->is_valid = StoredConfig::valid;
  }

  RtcBegin();            // Initialize the RTC chip
  ntpTimeClient.begin(); // Initialize the NTP client

  // Don't update the NTP time immediately here!!! Wait for the first loop() call!
  // Or the update interval will be too short and the initial call in the loop() will fail.

  // Set the sync provider for TimeLib to our syncProvider method
  setSyncProvider(&Clock::syncProvider);

  // Set TimeLib sync interval to current adaptive interval
  setSyncInterval(current_ntp_interval_ms / 1000); // This will make TimeLib call syncProvider() adaptively.

#ifdef DEBUG_NTPClient
  Serial.print("DEBUG_NTPClient: Initial NTP sync interval set to ");
  Serial.print(current_ntp_interval_ms / 1000);
  Serial.print("s (");
  Serial.print(current_ntp_interval_ms / 60000);
  Serial.println(" min)");
#endif
}

void Clock::loop()
{
  if (ntp_pending)
  { // Apply the NTP time fetched by the network task, corrected for the time since it arrived.
    ntp_pending = false;
    setTime(ntp_pending_time + (millis() - ntp_pending_millis) / 1000);
  }
  if (timeStatus() == timeNotSet)
  {
    time_valid = false;
  }
  else
  {
    loop_time = now();
    local_time = loop_time + config->time_zone_offset;
    time_valid = true;
  }
}

// Static methods used for sync provider to TimeLib library.
time_t Clock::syncProvider()
{
#ifdef DEBUG_OUTPUT_RTC
  Serial.println("DEBUG_OUTPUT_RTC: Clock:syncProvider() entered.");
#endif
  time_t rtc_now;

#ifdef DEBUG_NTPClient
  uint32_t current_millis = millis();
  Serial.print("\nDEBUG_NTPClient: Clock:syncProvider() - milis_last_ntp: ");
  Serial.println(millis_last_ntp);
  Serial.print("DEBUG_NTPClient: Clock:syncProvider() - current_ntp_interval_ms: ");
  Serial.println(current_ntp_interval_ms);
  Serial.print("DEBUG_NTPClient: Clock:syncProvider() - millis(): ");
  Serial.println(current_millis);
  Serial.print("DEBUG_NTPClient: Clock:syncProvider() - millis() - millis_last_ntp ");
  Serial.println(current_millis - millis_last_ntp);
  Serial.print("DEBUG_NTPClient: Clock:syncProvider() - millis_last_ntp == 0: ");
  Serial.println(millis_last_ntp == 0);
#endif

  rtc_now = RtcGet();
  return rtc_now;
}

void Clock::ntpLoop()
{
  if (millis() - millis_last_ntp < current_ntp_interval_ms && millis_last_ntp != 0) // Adaptive interval timing
    return;
  millis_last_ntp = millis(); // Store the attempt time, also on failure

  if (WifiState != connected)
  {
    Serial.println("NTP: no WiFi, keeping RTC time.");
    handleNtpFailure();
    return;
  }
  Serial.println("\nTime to update from NTP Server...");
  if (!ntpTimeClient.update())
  {
    Serial.println("NTP update query was not successful! Keeping RTC time.");
    handleNtpFailure();
    return;
  }
  time_t ntp_now = ntpTimeClient.getEpochTime();
  if (ntp_now < 1761609600) // Before 2025-10-28 00:00:01 UTC can't be valid
  {
    Serial.println("Time returned from NTP is not valid! Keeping RTC time.");
    handleNtpFailure();
    return;
  }
  Serial.printf("NTP time = %s (%ld)\n", ntpTimeClient.getFormattedTime().c_str(), (long)ntp_now);
  if (ntp_now != RtcGet())
  {
    Serial.println("RTC and NTP time differ, updating RTC time.");
    RtcSet(ntp_now);
  }
  ntp_pending_time = ntp_now;
  ntp_pending_millis = millis();
  ntp_pending = true; // Applied by loop() on the display core
  handleNtpSuccess();
}

uint8_t Clock::getHoursTens()
{
  uint8_t hour_tens = getHour() / 10;

  if (config->blank_hours_zero && hour_tens == 0)
  {
    return TFTs::blanked;
  }
  else
  {
    return hour_tens;
  }
}

// Adaptive NTP sync methods
void Clock::handleNtpSuccess()
{
  consecutive_failures = 0;
  consecutive_successes++;

#ifdef DEBUG_NTPClient
  Serial.print("DEBUG_NTPClient: NTP Success #");
  Serial.println(consecutive_successes);
#endif

  updateNtpInterval();
}

void Clock::handleNtpFailure()
{
  consecutive_successes = 0;
  consecutive_failures++;

#ifdef DEBUG_NTPClient
  Serial.print("DEBUG_NTPClient: NTP Failure #");
  Serial.println(consecutive_failures);
#endif

  updateNtpInterval();
}

void Clock::updateNtpInterval()
{
  uint32_t old_interval = current_ntp_interval_ms;

  if (consecutive_failures > 0)
  { // After failures: use error interval, increase with more failures
    if (consecutive_failures == 1)
      current_ntp_interval_ms = ntp_interval_error_ms; // 10 min
    else if (consecutive_failures >= 3)
      current_ntp_interval_ms = min(ntp_interval_error_ms * (1 << (consecutive_failures - 1)), ntp_interval_max_ms); // After 3+ failures: double the interval up to maximum
  }
  else if (consecutive_successes >= 6)
    current_ntp_interval_ms = ntp_interval_stable_ms; // After 6+ successes: move to stable interval (1 hour)
  else if (consecutive_successes >= 3)
    current_ntp_interval_ms = ntp_interval_normal_ms; // After 3+ successes: move to normal interval (30 min)
  else
    current_ntp_interval_ms = ntp_interval_initial_ms; // Initial or few successes: keep initial interval (5 min)

  // Update TimeLib sync interval if it changed
  if (old_interval != current_ntp_interval_ms)
    setSyncInterval(current_ntp_interval_ms / 1000);

#ifdef DEBUG_NTPClient
  if (old_interval != current_ntp_interval_ms)
  {
    Serial.print("DEBUG_NTPClient: NTP interval changed from ");
    Serial.print(old_interval / 1000);
    Serial.print("s to ");
    Serial.print(current_ntp_interval_ms / 1000);
    Serial.print("s (");
    Serial.print(current_ntp_interval_ms / 60000);
    Serial.println(" min)");
  }
#endif
}
