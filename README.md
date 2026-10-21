# EleksTubeHAX - EleksTube IPS Pro edition

![EleksTube IPS clock](/docs/ImagesMD/EleksTube_IPS_Classic_Edition.jpg)

This is a personal fork of [aly-fly/EleksTubeHAX](https://github.com/aly-fly/EleksTubeHAX), the aftermarket custom firmware for IPS "nixie" desk clocks.

Upstream supports many clock models and clones. **This fork targets only the EleksTube IPS Pro / Gen2 hardware**, and all code, build environments, docs and binaries for other models have been removed.
If you own a different clock, use the upstream project instead.

All credit for the original firmware goes to the upstream authors (see "Main Contributors" below). The project remains licensed under the GPL (see [LICENSE](/LICENSE)).

## 1\. Supported hardware

* **EleksTube IPS Clock Gen2** (incl. EleksTube IPS Classic Edition/Pro/PR1/PR2)
  * ESP32-PICO-D4, 4MB flash
  * 6x ST7789 135x240 IPS displays, selected via a 74HC595 shift register
  * 6x WS2812 RGB backlight LEDs
  * 4 buttons (Left, Mode, Right, Power)
  * DS3231 real-time clock with backup battery

##### Notes

* All "Original EleksTube" clocks sold after July 2022 are "Gen2" versions. Refer to the [Blog post on EleksTube website](https://EleksTube.com/blogs/news/instructions-on-EleksTube-clock-for-gen2-systems) for more details.

* Newer versions from EleksTube, such as PR1 and PR2, Pro and special editions (Pink etc.), are based on the base Gen2 version and sometimes called Gen3, because the original firmware version is already at Version 3.x for them.

## 2\. Mainboard/PCB view

EleksTube IPS - Gen2 (EleksTube IPS Classic Edition/Pro/PR1/PR2) ![EleksTube IPS clock - Gen2](/docs/ImagesMD/EleksTube_Gen2_PCB.jpg)

For detailed pictures see the `docs/Hardware EleksTube Gen2` subdirectory.

## 3\. Main features

* Show the actual time on the LCDs of the clock with the selected clock face

* Modes, switched with the MODE button: **Clock** and **Countdown** (`HH MM SS` up to 24 h; in the last minute "Oppenheimer" style `00 SS cc` with the hundredths racing at ~40 fps; backlight progress bar, alarm at zero; keeps running in the background). Claude and Music modes are planned, see [docs/DESIGN.md](docs/DESIGN.md).

* Buttons: MODE tap = next mode, hold = back to the clock, long hold (2 s) = settings menu. POWER tap = displays and backlights on/off, hold = backlights only. LEFT/RIGHT depend on the mode (countdown: tap to set the time, hold RIGHT to start/pause, hold LEFT to reset). Full map in [docs/DESIGN.md](docs/DESIGN.md#modes-and-button-map).

* On-Screen settings menu (MODE long hold) to change settings/configuration of the clock

* Multiple clock faces can be loaded into the clocks flash memory. Switchable via clock menu (or MQTT messages)

* 12/24 hour view (switchable via clock menu)

* WiFi connectivity with NTP server synchronization

* Supports either WPS connection or hardcoded WiFi credentials (from code)

* Automatic time zone and Daylight Saving adjustment using IP-based geolocation APIs (supports multiple providers)

* Optional manual time zone adjustment in 1 h and 15 minute slots (if IP Geolocation is disabled or fails)

### 3.2 Other Main Features

* RGB backlights (wall lights) for nice ambient light with multiple modes ("Off", "Test", "Constant", "Rainbow", "Pulse" and "Breath")

* Dimming of the displays and backlights during the night time (start and end time configurable in code)

* Turning displays on and off

* Keeping time even when power is off by using battery-powered real-time clock

* Saving and loading clock configuration from the flash, to store all settings, even when power is off

* Supports various bitmap image file variants (classic or palettized BMP) and proprietary compressed files (CLK)

* Maximum image size is 135 x 240 (Width x Height) pixels

* Supports smaller images – they will be automatically centered

* Advanced error handling for best user experience

* Supports Home Assistant integration with extensive MQTT support (see below for details)

* Supports custom MQTT integration (for example SmartNest.cz) for remote control - Switch clock faces and turn displays on/off

* With a MQTT broker (SmartNest, HiveMQ, SmartThings, Mosquitto etc.), this can also be integrated via a mobile phone app, a website or into an existing home automation network (and can be controlled via Google assistant, Alexa, etc.)

* WiFi and MQTT errors are displayed below the digits

* Debug outputs via the serial port

### 3.1 Home Assistant integration with extensive MQTT features

If activated in the code, clock can be remote controlled via Home Assistant - one of the leading free home automation solutions (see [Home Assistant Homepage](https://www.home-assistant.io)).

Features:

* Clock automatically appears in the active MQTT integration in HA (auto discovery)
* HA detects when the clock is powered off and grays out the entity.
* **Device detected as two separate light entities**  
  * Main (LCD displays)
  * Back (LED backlight)
* **Display control**
  * Turn LCDs on/off (*Main*)
  * Adjust LCD brightness (*Main*)
  * Change clock face/effect (*Main*)
* **Backlight control**
  * Turn LEDs on/off (*Back*)
  * Adjust LED brightness (*Back*)
  * Switch effects (Dark, Test, Constant, Pulse, Breathe or Rainbow) (*Back*)
  * Set static color (*Back*, Constant mode only)
  * Adjust effect speed (*Back*, for Pulse, Breathe and Rainbow)
* **Clock settings**
  * Toggle 12/24-hour format
  * Enable/disable blank zeros


## 4\. Quick Start - Backup and Pre-Build firmware images

Backup your original firmware and use the pre-built firmware to start with this custom firmware.

### 4.1 Backup first

**Always backup YOUR clocks firmware version as a first step!**

If you mess-up your clock, it's only your fault!

**Note**: **Backup images from other users** normally **DO NOT WORK** as the most original firmware (like from EleksTube) is locked to the MAC address of the ESP32!

For other clocks it MAY work, but don't assume it! Most clocks have MAC address binding!

So again: Do a backup of your original firmware first!

### 4.2 How to do the backup

#### 1) Install the USB Serial Port Device Driver

##### Windows

* On Windows, plug-in the cable into the clock and connect it to an USB port of your PC. Then run Windows Update. It will find and install the driver and generate an virtual COM port.  
    Windows device manager COM port example:  
    ![Windows device manager COM port example](/docs/ImagesMD/WindowsDeviceManagerCOMport.png)

* Only if Windows Update is not working for you, visit the Chinese manufacturer's website for newest [CH340 driver](https://www.wch-ic.com/downloads/CH341SER_ZIP.html) for Windows and install them manually.

##### Linux

* On an up-to-date Linux it works out of the box. COM port should be something like `/dev/ttyUSB0`.

* If it is not working, see this [tutorial](https://learn.sparkfun.com/tutorials/how-to-install-ch340-drivers/linux) and see the Chinese manufacturer's website for newest [CH340 driver](https://www.wch-ic.com/downloads/CH341SER_ZIP.html) for Linux.

#### 2) Save your original firmware

To read from (or write to) the clock, it needs to be in the "download mode". The clock goes into this mode automatically when the ESPTool tries to read or write to it.

##### Windows users

* In the `firmware` subdirectory of this repo you have the `esptool.exe`, which is used to talk to the ESP32 in your clock over the USB-Serial chip on the board of the clock.

* Open Device Manager and find out which virtual COM port represents your clock.

* Modify the backup script `_ESP32 save flash 4MB.cmd` with your COM port number.

* Run the CMD file.

* It will generate a file named `backup1.bin`. Rename it and save it to a safe place!

* This is your precious backup!

##### macOS / Linux users

* After `uv sync` and a first build (see section 5), PlatformIO ships its own `esptool.py`. Read the whole 4MB flash with:

  `uv run python ~/.platformio/packages/tool-esptoolpy/esptool.py --port /dev/cu.usbserial-XXXX --baud 460800 read_flash 0 0x400000 backup1.bin`

  (on Linux the port is something like `/dev/ttyUSB0`).

* Reading twice and comparing the checksums (`shasum -a 256`) is a cheap way to make sure the backup is not corrupted.

* Alternatively, this guide from the Tasmota project is very easy to follow: [ESPTool](https://tasmota.github.io/docs/Esptool/#put-device-in-firmware-upload-mode)

### 4.3 Use the pre-built firmware images

If you just want to use this firmware without setting up the development tools, download the latest firmware binary for your clock from the [Releases page](https://github.com/aly-fly/EleksTubeHAX/releases) and flash it using the `_ESP32 write flash.cmd` script in the `firmware` subdirectory.

Alternatively, the `firmware` subdirectory of this repo also contains pre-built firmware images that are updated from time to time.

More info can be found in the `firmware` subdirectory in the [README.MD](firmware/README.md). Like:

* Features enabled in the images

* How to flash the firmware file

If you want more features and configure the firmware, continue reading and check section "How to build this firmware".

### 4.4 Restore backup

You can write back your original firmware by modyfing the file `_ESP32 write flash.cmd`.

* Set correct COM port and the name of your firmware backup file

* If needed, set the size to be written

* Run the CMD file and flash the firmware file

* Check if clock is working

### 4.5 Known backup problems

#### COM port problems

There are a lot of possible issues known with so called "virtual COM ports". Some common ones are listed below.

* On Windows: No COM port at all, but "unknown device" in the device manager -> Try to install the CH340 drivers manually.

* ESPTool fails to read or write all of the firmware -> Check your cable and if possible try another and a shorter one. Try to reduce the baud rate of the connection (like to 115200 or even below). Try to use an external powered USB hub, to avoid under powering.

* ESPTool is giving an error like "device is not in the download mode" -> Try to hold the power button while plugging the USB-C (power) cable. If this doesn't help, you may have a timing problem with the USB-UART chip. This is known for some combinations of USB controllers (especially 3.x) and the CH340. Try to use a USB2.x HUB with external USB power supply or get another COM-port card. If nothing helps, try to use another computer.

For more info ask your favorite search engine or AI Chatbot :)

### 4.6 Lost firmware file

If you lost your original firmware file and wants to restore the original firmware you normally have a problem, because even if you get a backup file from another user, it is locked to the MAC address of the other ESP32 MAC. Under some conditions, it should be possible to change the encoded MAC address in the firmware. This worked for EleksTube Gen1 clocks. See [Issue 8](https://github.com/aly-fly/EleksTubeHAX/issues/8).

Note: There is no guarantee, that you are able to change the MAC address in the firmware file successfully! If the original manufacturer decides to encode the MAC address or hide it, the described method will not work!

So remember: **Always backup your original firmware!**

## 5\. How to build this firmware

Unfortunately, it's not simple plug-and-play. You need to do some things.

These instructions assume you already know how to use the **Visual Studio Code** and the **PlatformIO IDE** extension for it, and just need to know what to do.

### 5.0 Command line build with uv

The Python tooling (PlatformIO Core, Pillow for the image tools) is managed with [uv](https://docs.astral.sh/uv/) via `pyproject.toml`:

```sh
uv sync                                  # create .venv with PlatformIO
cp "include/_USER_DEFINES - empty.h" include/_USER_DEFINES.h   # then edit it
uv run pio run                           # build (default env: EleksTube_Gen2)
uv run pio run -t upload                 # flash the app partition over USB
uv run pio run -t uploadfs               # flash the clock faces (LittleFS) over USB
uv run pio device monitor                # serial monitor
scripts/ota.sh                           # build + upload firmware over WiFi
scripts/ota.sh filesystem                # build + upload the clock faces over WiFi
```

**Over-the-air updates.** With `OTA_ENABLED` in `_USER_DEFINES.h` the clock accepts uploads at `http://<device>.local/update` (HTTP basic auth, user `ota`). `scripts/ota.sh` reads `TUBEHAX_OTA_PASSWORD` (and optionally `TUBEHAX_CLOCK_HOST`) from `~/.config/tubehax/broker.env`. The tubes show an orange progress bar during the update; if an upload fails or the connection drops, the clock reboots into the firmware it was running. USB is only needed for the very first flash of this partition layout (or to restore the backup).

### 5.1 Download the code

You're either reading this file after downloading/cloning it already, or you're reading it on GitHub.

If the last point applies, then we assume you can figure out how to get the code from GitHub and put it somewhere on your local machine.

### 5.2 Visual Studio Code & PlatformIO IDE

Follow this guide here: [Install PlatformIO IDE](https://platformio.org/install/ide?install=vscode)

In short:

* Download, install and run VSCode

* Go to Extensions, search for "PlatformIO IDE" and install it (it will take a while - observe status messages in the bottom right corner). If you don't have Python already installed it will be automatically added by PlatformIO. In case of issues, install Python 3.x manually.

### 5.3 Open the project in Visual Studio Code/PlatformIO

* Start Visual Studio Code (VSC) and select "Open Folder".

* Select the project root `EleksTubeHAX` directory.

* PlatformIO (PIO) will detect the platformio.ini file, open the PlatformIO project, and download all needed packages/libraries.

* If this is not happening automatically, go to the PIO extension tab in VSC and select "Pick a Folder" and then select the `EleksTubeHAX` directory.

#### 5.3.1 ESP32 platform support

The EspressIF 32 development platform for PlatformIO is required to support the ESP32 microcontroller. It will be installed automatically when this project is opened in VSCode/PlatformIO or if the first build is triggered. It will take a while - observe status messages in the bottom right corner.

Tested on version 7.0.1 from the [PlatformIO registry](https://registry.platformio.org/platforms/platformio/espressif32).

It is possible that the project also works with ESP32 platform modules from other sources (like Tasmota or PIOArduino), but it is not recommended!

#### 5.3.2 PIO build environment

The only PIO build environment of this fork is `EleksTube_Gen2` (also the default), using the board definition of the original "Espressif ESP32 Dev Module" named `esp32dev`.

#### Partition sizes

Flash partition size settings are already configured in the following file.

| filename | environment | flash size | app part size | data part size |
| --- | --- | --- | --- | --- |
| `partition_4MB_ota.csv` | EleksTube_Gen2 | 4.0 MB | 2 x 1.5 MB (OTA) | 0.9 MB |

Two app partitions for over-the-air updates (the new firmware is written to the inactive slot), one data partition as LittleFS for the clock face and other assets.

These CSV files are used by PlatformIO to create the partitions on the flash of the ESP32 when uploading.

USB upload speed is set to 460800 baud in the `platformio.ini` file; 921600 baud gives "serial noise" errors on the Pro's CH340 USB-serial chip. If you still have upload issues, reduce it further.

#### 5.3.3 Libraries in use

All external libraries in use (for details, see the `platformio.ini` file) are automatically installed from the [PlatformIO registry](https://registry.platformio.org) (or their specified source locations). Since no fixed version numbers are set by default in `platformio.ini`, the **most recent versions** are fetched when the project is first opened or before the initial compilation.

* The initial installation may take some time - monitor status messages in the build log screen to see the progress.

* Standard libraries from the frameworks (espressif32 + arduino) are not explicitly listed.

##### 5.3.3.1 Verified Working Versions (as of 2026-06-07)

The project compiles and runs correctly with the library versions listed below. Newer (and possibly older) versions should also work.

If you encounter issues with automatic installation, refer to the comments in `platformio.ini` for troubleshooting.

##### 5.3.3.2 Used libraries

| Library | Author | Version | Source Code Link |
| --- | --- | --- | --- |
| adafruit/Adafruit NeoPixel | Adafruit | 1.15.5 | [https://github.com/adafruit/Adafruit\_NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel) |
| adafruit/RTClib | Adafruit | 2.1.4 | [https://github.com/adafruit/RTClib](https://github.com/adafruit/RTClib) |
| paulstoffregen/Time | Paul Stoffregen | 1.6.1 | [https://github.com/PaulStoffregen/Time](https://github.com/PaulStoffregen/Time) |
| bodmer/TFT\_eSPI *(standard registry version replaced by modified local copy, see 5.3.3.4)* | Bodmer | 2.5.43 | [https://github.com/Bodmer/TFT\_eSPI](https://github.com/Bodmer/TFT_eSPI) |
| knolleary/PubSubClient | Nick O'Leary | 2.8.0 | [https://www.arduinolibraries.info/libraries/pub-sub-client](https://www.arduinolibraries.info/libraries/pub-sub-client) |
| bblanchon/ArduinoJson | Benoit Blanchon | 7.4.3 | [https://github.com/bblanchon/ArduinoJson.git](https://github.com/bblanchon/ArduinoJson.git) |

##### 5.3.3.3 Library Code Used

Code from `dushyantahuja/IPGeolocation` library is copied into the project and heavily updated with bug fixes, error-catching, and support for multiple geolocation providers:

* **IP-API.com** (free, no API key required, 45 req/min limit)
* **Abstract API** (requires API key, 1,000 requests total limit)  
* **IPGeolocation.io** (requires API key, 1,000 requests/month limit)

Original libraries:

| Library | Author | Latest version | Source Code Link |
| --- | --- | --- | --- |
| dushyantahuja/IPGeolocation | Dushyant Ahuja | 3.0.0 | [https://github.com/dushyantahuja/IPGeolocation](https://github.com/dushyantahuja/IPGeolocation) |

##### 5.3.3.4 Modified Libraries

Some libraries require modifications to work with the specific clock hardware or to implement required functionality.  
The modified versions are stored in the `lib` subdirectory.

| Library | Author | Latest Version | Source Code Link |
| --- | --- | --- | --- |
| arduino-libraries/NTPClient | Arduino Libraries | 3.2.1 | [https://github.com/arduino-libraries/NTPClient](https://github.com/arduino-libraries/NTPClient) |
| bodmer/TFT\_eSPI (modified) | Bodmer | 2.5.43 | [https://github.com/Bodmer/TFT\_eSPI](https://github.com/Bodmer/TFT_eSPI) |

**Note:** A **modified version of `TFT_eSPI`** stored in `lib/modified_TFT_eSPI/` is used instead of the standard library fetched from the PlatformIO registry — the standard `bodmer/TFT_eSPI` entry in `platformio.ini` is intentionally commented out. The library is pre-configured by the `script_configure_tft_lib.py` build script, which copies `GLOBAL_DEFINES.h` as `User_Setup.h` into the modified library folder before compilation.

#### 5.3.4 Configure the `TFT_eSPI` library

The supplied `script_configure_tft_lib.py` automatically takes care of the library configuration. It copies two files (`_USER_DEFINES.h` and `GLOBAL_DEFINES.h`) into the `lib/modified_TFT_eSPI/` subdirectory before building. This makes sure that the modified local `TFT_eSPI` library is initialized with the correct values for the clock.

If you have issues with the scripts, copy the files manually every time the `TFT_eSPI` library is updated.

### 5.4 Configure, Build and Upload new firmware

Make sure you configured everything for your clock in the `_USER_DEFINES.h` in the `includes` subdirectory:

* Rename/Copy `_USER_DEFINES - empty.h` to `_USER_DEFINES.h`

* Select if you prefer WPS or hardcoded credentials for WiFi (comment '#define WIFI\_USE\_WPS' line and enter your WiFi network credentials to the other lines and uncomment them).

* Select image type for the clock faces to store: Bitmap files (.BMP) or .CLK files (uncomment #define USE\_CLK\_FILES).

Optionally:

* Enable automatic "Night Time Mode" by (uncomment `#define DIMMING` line and also the following which defines the begin and end of the night time).

* Enable integrated MQTT service (uncomment `#define MQTT_PLAIN_ENABLED` line and enter your MQTT credentials. From your local broker or from an internet-based broker. E.g. register on [SmartNest.cz](https://www.smartnest.cz/), create a Thermostat device, copy your username, API key and Thermostat Device ID.

* Enable Home Assistant (HA) support by uncomment `#define MQTT_HOME_ASSISTANT` and the following block of comments for Auto-Discovery in HA (local MQTT Broker is required).

* *NOTE*:
  * Only one MQTT service can be used at once.

* Disable/Change IP-based automatic timezone detection:

  This automatically sets your timezone offset when the clock starts and also detects Daylight Saving time switches.
  `#define GEOLOCATION_ENABLED` is active and IP-API.com is selected as default provider.

  Option 1:
  * IP-API.com (Recommended - No API key required)*
  * Free tier: 45 requests per minute, unlimited total usage
  * No registration required
  * Default provider when `GEOLOCATION_ENABLED` is uncommented
  * Use: `#define GEOLOCATION_PROVIDER_IPAPI` (enabled by default)
  * Leave `GEOLOCATION_API_KEY` empty or use placeholder value

  Option 2:
  * Abstract API (Alternative with API key)
  * Free tier: 1,000 requests total per account (no reset!)
  * Registration required at [Abstract API](https://www.abstractapi.com/)
  * Use: `#define GEOLOCATION_PROVIDER_ABSTRACTAPI`
  * Enter your API key in `GEOLOCATION_API_KEY`
  * **Warning**: Be careful not to exceed the 1,000 request limit!

  Option 3:
  * IPGeolocation.io (Alternative with API key)
  * Free tier: 1,000 requests per month
  * Registration required at [IPGeolocation.io](https://ipgeolocation.io/)
  * Use: `#define GEOLOCATION_PROVIDER_IPGEOLOCATION`
  * Enter your API key in `GEOLOCATION_API_KEY`
  
  *Important Notes:*
  * Only enable ONE provider at a time
  * The clock checks timezone on sunday at 3:00 AM (with retry protection)
  * Failed requests are limited to 4 attempts per day with 5-minute backoff
  * Timezone changes are validated (max 2-hour difference from stored value)

Connect the clock to your computer via a USB cable. You'll see, that a new serial port is detected and showing up in the device configuration. If not, check the section "Install the USB Serial Port Device Driver".

PlatformIO will automatically select the right port for uploading (in most cases).

The clock goes into the download mode automatically when PlatformIO is trying to upload the built firmware files.

**Note**: If you have Bluetooth virtual ports on your machine, it might hang and you must manually select the COM port in the `platformio.ini`, see [Upload options](https://docs.platformio.org/en/latest/projectconf/sections/env/options/upload/index.html).

#### 5.4.1 Step 1 - Compile the code and upload the firmware file (to the app partition)

Compile the code via the "Build" command of PlatformIO extension for the `EleksTube_Gen2` environment and upload the code via the "Upload" command (or use the `uv run pio` commands from section 5.0).

![PlatformIO Build](/docs/ImagesMD/PlatformIOBuild.png)

At this point, it should build cleanly and upload successfully.

Example output from a successful build:

![PlatformIO Build Output](/docs/ImagesMD/PlatformIOBuildOutput.png)

Example output from a successful upload:

![PlatformIO Upload Output](/docs/ImagesMD/PlatformIOUploadOutput.png)

**Note**: You'll see the clock boot up after upload. It will go into the setup routine and ask for WPS or connecting to the configured WiFi network.

**Note**: After the initially flash, your clock will **NOT SHOW ANYTHING** on the displays after the setup phase! This is, because it doesn't have any bitmaps to display on the flash memory yet!

**The screens will stay blank until you upload data!**

Continue with the next step to get the clock running.

#### 5.4.2 Step 2 - Fill data partition (LittleFS) with images

The repository comes with a "predefined" set of BMP files in the `data` subdirectory.

Each set of 10 images (one image for each digit) is called a "clock face" and can be chosen from the clock menu. Each set represents normally a different design or 'font' for the clock. See below if you want to make your own.

Note: All files in the `data` subdirectory will be packed into the LittleFS flash image! Make sure to tidy it up. Only the necessary images should be there and `clockfaces.txt` with names of the clock fonts for HA.

##### 5.4.2.1 Generate and upload

* In PlatformIO extension go to "Project Tasks" and expand: EleksTubeHax -> Platform for the clock environment.

* Select "Build Filesystem Image" first.

* Then connect the clock in download mode.

* Click "Upload Filesystem Image".

![PlatformIO Build Filesystem](/docs/ImagesMD/PlatformIOBuildFilesystem.png)

This will upload the files to the LittleFS filesystem on the ESP32 (flash of the clock).

Note: The data will stay there, even if you re-upload the real firmware to the app partition, because the data partition is not overridden or modified by that.

### 5.4.3 Check if clock is working

After Step 2 the clock should boot up and show the boot sequence on all displays. After finishing the boot sequence, the clock should display the actual time with the default clock face.

## 5.5 Create a flashable firmware image

Alternatively to build and uploading the app part and the data part in single steps, you can create a single firmware file, which is flashable all at once to the clock.

You still need to define all the settings/features you want to use in the `USER_DEFINES.h`.

Additionally, you need to uncomment the line `-D CREATE_FIRMWAREFILE` in the platformio.ini to enable a post-build step, to create the image for your clock.

If you now build the project in PlatformIO via the "Build" command, the normal build will takes place and afterwards, a helper script will call the build for the LittleFS data partition and then merge the existing single files together to one file.

The output file is written to the default output dir of the build. Usually the subdirectory `.pio\build\<environment name>` in the project directory.

The firmware file will be named like `FW_<CLOCKNAME>_<VERSIONNUMBER>.bin` (e.g., `FW_EleksTube_Gen2_v1.3.13.bin`).

This file can be flashed with the `esptool.exe` with the `write_flash` option.

E.g. assuming you are using the `esptool.exe` in the `firmware` subdirectory and the clock is connected to COM5:

`esptool.exe --chip esp32 --port COM5 --baud 921600 --before default_reset --after hard_reset write_flash 0x0000 ..\.pio\build\EleksTube_Gen2\FW_EleksTube_Gen2_v1.3.13.bin --erase-all`

### 5.5.1 Helper scripts

#### 5.5.1.1 Unified firmware binary builder (`script_build_unified_binary.py`)

The legacy `script_build_fs_and_merge.py` has been replaced by `script_build_unified_binary.py`. This post-build helper registers two chained PlatformIO actions whenever `CREATE_FIRMWAREFILE` is defined.

First, it launches PlatformIO with the `buildfs` target for the active environment (same result as choosing "Build Filesystem Image" in the GUI) so a fresh `littlefs.bin` lands in the environment's build directory.

Second, it parses the currently selected partition CSV to discover the offsets, gathers the core images (bootloader, partitions, app) and the freshly built filesystem image, and then calls `python -m esptool merge-bin` (with a fallback to the legacy `merge_bin` syntax) to emit the unified firmware file.


## 5.6 Miscellaneous stuff

### 5.6.1 Custom clock faces from Bitmaps

If you want to change the uploaded clock faces for the clock:

* This fork ships a single face: the stock EleksTube nixie face, extracted from the original firmware. Its source JPEGs are in `assets/clockface_stock_nixie`; regenerate the BMPs with `uv run python tools/convert_stock_face.py`.

* Create your own set of BMP files or download some from the internet (see below).

* Maximum resolution of each image is 135 x 240 pixels (HxW). They can be smaller, then the picture will be centered on the display.

* Maximum color depth is 24 bit RGB. But recommended is palettized Bitmaps with 256 colors palette.

* Name them `10.bmp` (for digit Zero) through `19.bmp` (for digit Nine); `20.bmp` to `29.bmp`, and so on. Note: There is no set 00-09.bmp!

* You can add max 8 clock face sets in the moment (Due to a problem in the detection mechanism).

* If needed, rename the generated files and put them in the `data` subdirectory.

* Then do the "Build Filesystem Image & Upload Filesystem Image" dance again.

Tips:

* Cut away any black border, this only eats away valuable flash storage space! Keep the same height for all images!

* Run your preferred image editor and play with reduced bit depths / palettization and dither of the image!

* Very good results are with Dithering and 256-color palette. Size reduction is approx 70%.

* With very simple images (like 7-segment digits) even 16-color (4-bit) palette is enough and reduces size even further.

### 5.6.2 CLK files as alternative

Before supporting palettized Bitmaps, there was a special format used to store images, to safe some space on the flash of the clock. You can still switch to use CLK (Clock) files only, but there is no space saving anymore, if palettized Bitmaps are used!

To convert existing image files to CLK format:

**Option A — Windows GUI tool** (`\tools\Prepare_images\Convert_BMP_to_CLK.exe`, Windows only):

* Run the tool and select all BMP files to be converted at once. It will create CLK files from them.

**Option B — Python script** (`tools/conv-bmp-to-clk.py`, cross-platform):

* Requires Python 3 with the `Pillow` library (`pip install pillow`).

* Run from the project root, passing the folder that contains your BMP files:

  ```
  python tools/conv-bmp-to-clk.py data
  ```

* By default the converted CLK files are written to a subfolder named `clk` next to the input folder. Use `--out <folder>` to choose a different output location:

  ```
  python tools/conv-bmp-to-clk.py data --out data_clk
  ```

* The script converts all `.bmp` files in the given folder (non-recursive) to RGB565 CLK format and preserves the original filenames.

For both options:

* For a 24-bit depth Bitmap, size reduction is approx 30%.

* If needed, rename the generated files and put them in the `data` subdirectory.

* Then do the "Build Filesystem Image & Upload Filesystem Image" dance again.

Note: It is either Bitmap or CLK! No mixing, so make sure to "clean" the `data` subdirectory before switching.

#### 5.6.3 Download Clock faces

Here are links to some good 3rd party sets out there:

* [https://github.com/upiir/ips\_clock\_100x\_themes](https://github.com/upiir/ips_clock_100x_themes)

* [https://github.com/upiir/EleksTube\_ips\_custom\_theme](https://github.com/upiir/EleksTube_ips_custom_theme)

* [https://github.com/upiir/rgb\_glow\_tube\_clock](https://github.com/upiir/rgb_glow_tube_clock)

If you have your own clock face that'll work and want it listed here, please file an Issue and/or Pull Request.

#### 5.6.4 Configure your WiFi network

* For WPS: When prompted by the clock, press WPS button on your router (or in web-interface of your router). Clock will automatically connect to the WiFi and save data for future use. No need to input your credentials anywhere in the source code. The clock will remember WiFi connection details even if you unplug the clock.

* Without WPS: Add your WiFi credentials into `_USER_DEFINES.h` file before building the firmware.

Note: The `_USER_DEFINES.h` is included in the default `.gitignore` file, so that your personal credentials will not be pushed to a git repo by default, if you are using a forked git repo. But note, that the credentials can be found as strings in the firmware .BIN file.

### 5.6.5 MQTT and Home Assistant

#### 5.6.5.1 Home Assistant

#### 5.6.5.1.1 Setup

Note: A lot of changes has been made on the HA integration with V1.3.0. User action is requiered to 'migrate' the devices. Detailed information can be found in the section 5.6.5.1.2 "Migration Notes (Home Assistant)".

If you want to integrate the clock into your Home Assistant, you need to make sure, that Home Assistant and the clock uses the same MQTT broker.

Normally you will already have a MQTT broker running locally, which supports the HA discovery and communication messages, like Mosquitto. If not done already, you can set it up easily via an Add-On in HA. See: [Home Assistant MQTT Integration](https://www.home-assistant.io/integrations/mqtt/).

You can also use an internet-based broker which supports "Home Assistant messages" (topics) for discovery and communication, like HiveMQ.

Enable the `MQTT_HOME_ASSISTANT` define to have HA support and the belonging `MQTT_HOME_ASSISTANT_*` defines for auto discovery via the MQTT integration.

After you set up the broker, you need to enter the used IP, port, username and password of it into the matching `MQTT_*` defines in your `_USER_DEFINES.h` (MQTT config section).

After flashing the firmware, the clock will first try connect to the WiFi and then to the given MQTT broker. Check the displays, if a 'NO MQTT' message appears under one of the digits, then the connection was NOT successful and you need to debug why. Hint: use a switch `#define DEBUG_OUTPUT_MQTT` to get detailed debug messages on the serial port of the clock.

If the connection was successful the clock sends the discovery messages for HA and the device should be visible in the MQTT integration.

Note: If you change your used clock faces in the `data` subdirectory, the `clockfaces.txt` in the `data` subdirectory needs to be changed too. This is, because the shown names in HA for the clock faces under "Main light" are hard coded and read from there.

No manual `MQTT_CLIENT` define is needed. The firmware now auto‑generates a stable, lower‑case device identifier from hardware data (MAC/model) and uses the namespace pattern `elekstubehax/<device_id>/...`. Ignore older documentation references instructing you to set `MQTT_CLIENT`.

Note: For using smartnest.cz, define `MQTT_CLIENT_ID_FOR_SMARTNEST` (previously referred to as `MQTT_CLIENT`) to match the external service's required fixed device ID. This becomes the root of the published topics expected by that service.

Note: If you want to use an internet-based broker, you can use HiveMQ. You will need to create an account there and set it up in HA and in here. 'MQTT\_USE\_TLS' must be defined, because HiveMQ only supports encrypted connections. The HiveMQ TLS cert is based on the root CA of Let's Encrypt, so you also need to copy the 'mqtt-ca-root.pem' file from the `data/other graphics` subdirectory into the `data` subdirectory of the PIO project and upload the data partition (file system) with the changed app partition. Other brokers or your locally used cert for your MQTT broker may need another root CA to be set. See: [Connect HA to HiveMQ](https://www.hivemq.com/blog/connect-home-assistant-to-hivemq-cloud/).

Note: If you use Mosquitto and want to separate the permissions on the topics or experience access problems in the logs, you need to set the ACLs manually for it! Commands for it:
user <your_mqtt_username>
topic read homeassistant/status
pattern readwrite elekstubehax/%c/#
pattern readwrite homeassistant/+/%c/#

#### 5.6.5.1.2 Migration Notes (Home Assistant)

This section centralizes all earlier inline migration notes previously shown in other subsections.

Summary:

| Change | What Changed | Action Needed | Impact If Ignored |
| ------ | ------------ | ------------- | ----------------- |
| New device ID | `device.identifiers`, `unique_id`, and related values are now constructed from the device model plus the last three tuples of the MAC address | Delete old entities and retained discovery topics. Adjust all HA references (Dashboards, Automations, etc.) to the new IDs | Old entities remain orphaned; dashboards show invalid cards; automations may fail |
| New topics for status messages | All runtime status topics are namespaced under `elekstubehax/` with per‑device subtopics using the lower‑case `device_id` | Delete old retained status topics if present (to avoid confusion) | You may see stale/unused topics alongside the new ones |
| New topics for discovery messages | Exactly one discovery config per device and HA domain (light, number, switch) using the lower‑case `device_id` path | Delete old ghost entities and their retained discovery topics | Old entities remain orphaned and can reappear from retained configs |
| Optional device name suffix | Plain `Model Name` vs. `Model Name (ABCDEF)` when `ENABLE_HA_DEVICE_NAME_SUFFIX` defined | None (opt‑in). To see new/removed suffix immediately, clear retained discovery topics | HA may show old cached name until refresh |

Details:

1. New device ID format and separator change:
  The firmware now constructs a stable `device_id` from the model name plus the last three MAC‑address bytes (example: `elekstubeg2-1a2b3c`). Discovery `unique_id`/`object_id` use underscores instead of slashes (e.g., `myclock123_front` instead of `myclock123/front`). Home Assistant treats changed `unique_id` values as new entities. After flashing a version with this scheme, delete obsolete entities in HA (Settings → Devices & Services → MQTT → device → three‑dot menu → Delete) and remove their retained discovery config topics so they do not reappear.

2. New topics for status messages:
  All runtime (non‑discovery) topics are published under the root `elekstubehax/<device_id>/...` using the lower‑case `device_id`. This yields unique, per‑device status and command channels (for example: `elekstubehax/elekstubeg2-1a2b3c/front` and `/front/set`). If you previously subscribed to older topic paths, update any external scripts/dashboards. Consider deleting old retained status topics to avoid confusion when browsing.

3. New topics for discovery messages:
  Each device now publishes exactly one discovery config per HA domain (light, number, switch) under the path `homeassistant/<domain>/elekstubehax/<device_id>/<entity>/config`. Paths include the lower‑case `device_id`. If retained configs from older paths exist, delete them to prevent ghost entities from reappearing in HA after restarts.

4. Optional device name suffix (cosmetic):
  By default the firmware publishes the plain hardware model in `device.name` (e.g., `EleksTube IPS Tube Clock`). Defining `ENABLE_HA_DEVICE_NAME_SUFFIX` appends a short identifier `(XXYYZZ)` built from the last three MAC bytes, e.g., `EleksTube IPS Tube Clock (3A4FBC)`. `device.identifiers` and all entity `unique_id` values remain unchanged. Use the suffix only if you run several identical clocks and want quick visual disambiguation.

5. Cached device.name after toggling the suffix:
  HA may cache the previous device name until it processes an updated retained discovery message. If you add or remove the suffix flag and want the UI updated immediately, follow the refresh procedure below.

6. General refresh procedure:
  a. Power off / disconnect MQTT for the clock.
  b. Delete all (old) retained discovery config topics for that device:
    like `homeassistant/light/<device_id>/main/config` or `homeassistant/light/<device_id>_main`, `.../back/config` or `..._back/config`, corresponding `homeassistant/switch/...` and `homeassistant/number/...` topics.  
  c. Flash the Firmware with the new MQTT implementation (>1.2.x)
  d. Power the device back on; it publishes the discovery.
  e. Verify in HA device list.
  f. Delete the references to the old device in HA or change them to the new ones.

##### 5.6.4.1.2 Used integrations

Interactions between the firmware of the clock and Home Assistant is done as described in the MQTT integration documentation (see below).

By default, the clock sends auto-discovery messages, and all its entities will be detected by the MQTT integration without user intervention.

The clock is discoverd as two lights (Main and Back) and some other entities (i.e. a switch) as Configuration. The last one is needed to be able to switch the settings of the clock.

See the HA MQTT documentation for details of the used MQTT integrations:

* [MQTT Discovery](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery)

* [MQTT Light integration](https://www.home-assistant.io/integrations/light.mqtt/)

* [MQTT Switch integration](https://www.home-assistant.io/integrations/switch.mqtt/)

* [MQTT Number integration](https://www.home-assistant.io/integrations/number.mqtt/)

##### 5.6.4.1.3 Usage

After a successful auto discovery, you will find a new device under "Setting -> Devices and Services -> MQTT -> XX devices", named after the auto‑generated device id (the same lower‑case identifier visible in the MQTT topic paths). A manual `MQTT_CLIENT` define is NOT required for Home Assistant integration.

![HA MQTT Integration Overview](/docs/ImagesMD/HAMQTTIntegrationOverview.png)

If you go into the device details it should look like this:

![HA MQTT Integration Details](/docs/ImagesMD/HAMQTTIntegrationDetails.png)

You can now use this device on your HA dashboards.

The main light represents the displays of the clock, the back light the LED ambient lights.

Each one is dimmable, can be turned on and off and for the 'main light', the clock faces are switchable via "Effect".

![HA MQTT Main Light Details](/docs/ImagesMD/HAMQTTMainLightDetails.png)

For the 'back light', the LED colour and mode can be set.

![HA MQTT Back Light Details](/docs/ImagesMD/HAMQTTBackLightDetails.png)

The settings for the LED modes and the general clock settings can only be set from the "Configuration" entity.

#### 5.6.4.1.4 MQTT Discovery, Topic Layout and Naming Conventions

This firmware supports automated entity creation in Home Assistant via the MQTT Discovery mechanism. All topics follow a consistent, lower‑case namespace to avoid duplication caused by case changes and to simplify manual scripting.

Key principles:

- Root namespace for all runtime state and commands: `elekstubehax`
- Per‑device path segment: lower‑cased unique device name – referred to as `device_id`. Format: `<model>-<XXYYZZ>` where `<model>` is the compile‑time `DEVICE_NAME` (`elekstubeg2` for this fork) and `<XXYYZZ>` are the last three bytes of the ESP32 MAC address in hexadecimal (bytes 3–5 of the canonical MAC). Example: `elekstubeg2-1a2b3c`. This keeps IDs short while remaining sufficiently unique for typical home deployments.
- Entity channel/topic keyword ("main", "back", "use_twelve_hours", "blank_zero_hours", "pulse_bpm", "breath_bpm", "rainbow_duration") – referred to as `entity_id`
- Discovery identifiers (`unique_id` and `object_id`) now use underscore separators: `<device_id>_<entity_id>` (no slashes)
- All state, attribute and command topics are JSON where applicable (schema `json` for lights) or key/value JSON for switches and numbers

Topic patterns (runtime):

```text
Availability:         elektstubehax/<device_id>/alive
Light (main) state:   elektstubehax/<device_id>/main
Light (main) cmd:     elektstubehax/<device_id>/main/set
Light (back) state:   elektstubehax/<device_id>/back
Light (back) cmd:     elektstubehax/<device_id>/back/set
Switch 12h state:     elektstubehax/<device_id>/use_twelve_hours
Switch 12h cmd:       elektstubehax/<device_id>/use_twelve_hours/set
Switch blank0 state:  elektstubehax/<device_id>/blank_zero_hours
Switch blank0 cmd:    elektstubehax/<device_id>/blank_zero_hours/set
Number pulse state:   elektstubehax/<device_id>/pulse_bpm
Number pulse cmd:     elektstubehax/<device_id>/pulse_bpm/set
Number breath state:  elektstubehax/<device_id>/breath_bpm
Number breath cmd:    elektstubehax/<device_id>/breath_bpm/set
Number rainbow state: elektstubehax/<device_id>/rainbow_duration
Number rainbow cmd:   elektstubehax/<device_id>/rainbow_duration/set
```

Discovery topics (published once per entity or after HA restart):

```text
homeassistant/light/<device_id>/main/config
homeassistant/light/<device_id>/back/config
homeassistant/switch/<device_id>/use_twelve_hours/config
homeassistant/switch/<device_id>/blank_zero_hours/config
homeassistant/number/<device_id>/pulse_bpm/config
homeassistant/number/<device_id>/breath_bpm/config
homeassistant/number/<device_id>/rainbow_duration/config
```

Example discovery payload (front light) – abbreviated:

```json
{
  "unique_id": "elekstubeg2-1a2b3c_main",
  "object_id": "elekstubeg2-1a2b3c_main",
  "name": "Main",
  "schema": "json",
  "availability_topic": "elekstubehax/elekstubeg2-1a2b3c/alive",
  "state_topic": "elekstubehax/elekstubeg2-1a2b3c/main",
  "json_attributes_topic": "elekstubehax/elekstubeg2-1a2b3c/main",
  "command_topic": "elekstubehax/elekstubeg2-1a2b3c/main/set",
  "supported_color_modes": ["brightness"],
  "brightness": true,
  "effect": true,
  "effect_list": ["ClockFace1", "ClockFace2", "…"],
  "device": {
  "identifiers": ["EleksTube-1A2B3C"],
    "manufacturer": "EleksTubeHAX",
    "model": "IPS Clock",
    "sw_version": "<firmware-version>"
  }
}
```

Switch command payload examples:

```json
{"state":"ON"}
```

```json
{"state":"OFF"}
```

Number command payload example (pulse speed 75):

```json
{"state":75}
```

Light (front/back) command payload examples (subset):

```jsonc
// Turn front display off
{"state":"OFF"}

// Set brightness (0..255 for front, 0..255 for back) and effect
{"state":"ON","brightness":180,"effect":"Rainbow"}

// Set static backlight color (hs mode) + brightness
{"state":"ON","color":{"h":210,"s":100},"brightness":150,"effect":"Constant"}
```

Effects:

* Front light: effect list equals available clock faces.
* Back light: effect list equals firmware‑defined patterns (Dark/Test/Constant/Pulse/Breath/Rainbow etc.).

Entity summary:

| Entity | HA Domain | Purpose |
| ------ | --------- | ------- |
| front  | light     | LCD tubes (brightness + clock face as effect) |
| back   | light     | Ambient LEDs (brightness, color HS, effects) |
| 12hr   | switch    | 12/24 hour mode toggle |
| blank0 | switch    | Blank leading zero of hour |
| pulse  | number    | Pulse effect BPM (20–120) |
| breath | number    | Breath effect BPM (5–60) |
| rainbow| number    | Rainbow cycle duration seconds (0.2–10) |

Migration note (important): Prior firmware versions used slash separators in `unique_id` / `object_id` (e.g., `myclock123/front`). Home Assistant treats a changed `unique_id` as a brand‑new entity; the old one will remain in the entity registry until manually removed. If you previously had entities with the old format, you can safely delete the obsolete entries in Home Assistant (Settings → Devices & Services → MQTT → device → three‑dot menu → Delete) after the new ones appear.

Automation tips:

- Use `availability_topic` to gate automations (`state == 'online'`).
- Prefer referencing entities via their entity_id instead of hardcoding MQTT topics when working inside Home Assistant.
- For external scripts, build topics exactly as documented to avoid mismatches after firmware updates.

If you disable Home Assistant support (`MQTT_HOME_ASSISTANT` not defined), discovery messages are not sent, but plain MQTT state/command topics stay available (if `MQTT_PLAIN_ENABLED` is defined). See also next chapter.

#### 5.6.5.2 MQTT without Home Assistant

##### 5.6.5.2.1 Setup

If the `MQTT_HOME_ASSISTANT` is not enabled, but `MQTT_PLAIN_ENABLED` is, the MQTT support is limited. This is mostly intended to work with [SmartNest.cz](https://www.smartnest.cz/) service, which is a simple user interface. It can be extended / integrated with Google assistant, Alexa or SmartThings.

The actual MQTT implementation is reacting to control commands of an emulated temperature controller, so you can use the temperature setpoint to change clock faces and on/off function to control the clock.

You will need to create an account, set up the device there and get the ID of it; and fill in the data into the matching `MQTT_*` defines in your `_USER_DEFINES.h` (MQTT config section).

Important: Uncomment and define `MQTT_CLIENT_ID_FOR_SMARTNEST` or the topic is not correct for smartnest.cz.

Note: If you want to use encrypted connection to your broker, you need to enable 'MQTT\_USE\_TLS' and copy a valid root CA cert file to the `data` subdirectory. See the notes under the HA section for HiveMQ above.

#### 5.6.5.3 Debugging MQTT

Further analysis or debugging problems with the MQTT code is easier to perform using a MQTT message reader tool, for example, "MQTT Explorer", a common HA Add-On for users, who prefer manual configuration. See [HA forum post for MQTT Explorer Add-On](https://community.home-assistant.io/t/addon-mqtt-explorer-new-version/603739).

Note: It can be also used as a stand-alone application. See [Homepage of MQTT Explorer project](https://mqtt-explorer.com/).

You need to connect to the same MQTT broker like the clock and then will be able to see the messages from the clock and send them.

All MQTT messages from and to the clock are also traced out via the serial interface. So using a serial monitor while using the clock, gives also debug information. Make sure you enable the `DEBUG_OUTPUT_MQTT` before compilation and upload.

## 6\. Development Process/History

See [Old Readme File](/README_OLD.md) for details.

[Reddit discussion on the original hack is here.](https://www.reddit.com/r/arduino/comments/mq5td9/hacking_the_EleksTube_ips_clock_anyone_tried_it/)

[Original documentation and software from EleksMaker.](https://wiki.eleksmaker.com/doku.php?id=ips)

There was an offical "manual" and software package avaiable for the original EleksTube clock. The website is not maintained anymore, so just for archive purpose: See [EleksTube instructions](https://wiki.eleksmaker.com/doku.php?id=ips).

## 7\. Main Contributors

* Mark Smith, aka Smitty ... @SmittyHalibut on GitHub, Twitter, and YouTube.

* Aljaz Ogrin, aka aly-fly ... @aly-fly on GitHub and Instagram

* Clemens Sutor, aka Martinius79 ... @Martinius79 on GitHub

* Misc code snips either committed by or copied from: @icebreaker-ch, @meddle99, @OggyP, @bitrot-alpha

* Home Assistant support by @victorvuelma and @gamba69

* Help with MarvelTube Mini integration by @Hans99

* in future (on to-do list) also from: @RedNax67, @wfdudley, @judge2005

*Happy hacking!*
