/*
 * Author: Aljaz Ogrin
 * Project: Alternative firmware for EleksTube IPS clock
 * Hardware: ESP32
 * File description: Global configuration for the complete project
 *   Should only need editing to add a new harware version of the clock
 *   User configuration is located in "_USER_DEFINES.h"
 */

#pragma once

#ifndef GLOBAL_DEFINES_H_
#define GLOBAL_DEFINES_H_

#include <Arduino.h>
#include <stdint.h>
#include "_USER_DEFINES.h"

#define VALUE(string) #string
#define TO_LITERAL(string) VALUE(string)

// ************* Type of the clock hardware  *************
// This fork targets only the EleksTube IPS Pro / Gen2 hardware
// (EleksTube IPS Classic Edition/Pro/PR1/PR2, ESP32 Pico D4 chip, 4MB flash).

// ************* Version Infomation  *************
#define FIRMWARE_VERSION TO_LITERAL(BUILDVER)
#define SAVED_CONFIG_NAMESPACE "configs"

// ************ WiFi advanced config *********************
#define ESP_WPS_MODE WPS_TYPE_PBC
#define ESP_MANUFACTURER "ESPRESSIF"
#define ESP_MODEL_NUMBER "ESP32"
#define ESP_MODEL_NAME "IPS clock"

#ifndef CONFIG_ESP32_WIFI_NVS_ENABLED
#define CONFIG_ESP32_WIFI_NVS_ENABLED 1 // Force NVS usage for WiFi driver
#endif                                  // CONFIG_ESP32_WIFI_NVS_ENABLED

// ************ MQTT config *********************
#define MQTT_REPORT_STATUS_EVERY_SEC 15 // How often report status to MQTT Broker

// ************ Backlight config *********************
#define DEFAULT_BL_RAINBOW_DURATION_SEC 8

// ************ Hardware definitions *********************

// Disable all warnings from the TFT_eSPI lib
#define DISABLE_ALL_LIBRARY_WARNINGS

// Common indexing scheme, used to identify the digit
#define NUM_DIGITS (6)
#define SECONDS_ONES (0)
#define SECONDS_TENS (1)
#define MINUTES_ONES (2)
#define MINUTES_TENS (3)
#define HOURS_ONES (4)
#define HOURS_TENS (5)

#define SECONDS_ONES_MAP (0x01 << SECONDS_ONES)
#define SECONDS_TENS_MAP (0x01 << SECONDS_TENS)
#define MINUTES_ONES_MAP (0x01 << MINUTES_ONES)
#define MINUTES_TENS_MAP (0x01 << MINUTES_TENS)
#define HOURS_ONES_MAP (0x01 << HOURS_ONES)
#define HOURS_TENS_MAP (0x01 << HOURS_TENS)

// Define the activate and deactivate state for the display power transistor and how the dimming value is calculated.
#define ACTIVATEDISPLAYS HIGH   // Activate is HIGH
#define DEACTIVATEDISPLAYS LOW  // Deactivate is LOW
#define CALCDIMVALUE(x) (x)     // Dimming value is directly used for software dimming

#define DIGIT_CS_ACTIVE_LEVEL HIGH
#define DIGIT_CS_INACTIVE_LEVEL LOW

/*************************
 *    EleksTube Gen2     *
 *************************/
#define DEVICE_NAME "EleksTubeG2"
#define DEVICE_MANUFACTURER "EleksMaker"
#define DEVICE_MODEL "EleksTube IPS Tube Clock - Gen2"
#define DEVICE_HW_VERSION "Gen2"

// WS2812 (or compatible) LEDs on the back of the display modules.
#define BACKLIGHTS_PIN (12)
#define NUM_BACKLIGHT_LEDS (6) // 6 LEDs, one each on the back of every LCD

// Buttons, active low, externally pulled up (with actual resistors!)
#define BUTTON_LEFT_PIN (33)
#define BUTTON_MODE_PIN (32)
#define BUTTON_RIGHT_PIN (35)
#define BUTTON_POWER_PIN (34)

// I2C to DS3231 RTC.
#define RTC_SCL_PIN (22)
#define RTC_SDA_PIN (21)

// Chip Select shift register, to select the display.
#define CSSR_DATA_PIN (14)
#define CSSR_CLOCK_PIN (9)
#define CSSR_LATCH_PIN (10)

// SPI to displays is defined User_Setup.h file.
// Look for TFT_MOSI, TFT_SCLK, TFT_CS, TFT_DC, and TFT_RST defines.

// Power for all TFT displays are grounded through a MOSFET so they can all be turned off.
// Active HIGH.
#define TFT_ENABLE_PIN (27)

// Configure library \TFT_eSPI\User_Setup.h: ST7789 135 x 240 display with no chip select line.
#define ST7789_DRIVER // Configure all registers
#define TFT_WIDTH 135
#define TFT_HEIGHT 240
#define CGRAM_OFFSET // Library will add offsets required
#define TFT_SDA_READ // Read and write on the MOSI/SDA pin, no separate MISO pin
#define TFT_MOSI 23
#define TFT_SCLK 18
// #define TFT_CS -1  // Not connected
#define TFT_DC 25  // Data Command, aka Register Select or RS
#define TFT_RST 26 // Connect reset to ensure display initialises

// Fonts to load for TFT.
// #define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2 // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4 // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
// #define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
// #define LOAD_FONT7  // Font 7. 7-segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:.
// #define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
// #define LOAD_FONT8N // Font 8. Alternative to Font 8 above, slightly narrower, so 3 digits fit a 160 pixel TFT
// #define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

#define SMOOTH_FONT // MUST REMAIN ACTIVE OTHERWISE BUTTON CONFIG IS CORRUPTED for some reason....

#define SPI_FREQUENCY 40000000

/*
 * Force the TFT_eSPI library to not over-write all this:
 */
#define USER_SETUP_LOADED

#endif /* GLOBAL_DEFINES_H_ */
