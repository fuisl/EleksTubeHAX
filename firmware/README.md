# EleksTubeHAX - An aftermarket custom firmware for the desk clock

This fork targets only the **EleksTube IPS Pro / Gen2** hardware (ESP32 Pico D4, 4MB flash).

## 1. Pre-built firmware file

In this folder you can find a pre-built firmware image file that you can directly upload/flash to your clock.

This binary may have limited functionality compared to a self-built firmware, because not all available features are enabled or can be configured.

The image files (clock faces) are fixed and can't be changed without building your own firmware file with the development tools (see README.md)!

### 1.1 Pre-configured functionality

Enabled:

- WiFi connectivity via WPS (While clock is in boot phase, press WPS button on your router for WiFi setup).
- Night time dimming enabled - from 22:00h (10 pm) in the evening to 07:00h (7 am) in the morning.
- IP-based geolocation enabled - time zone & DST comes from IP-API.com.

Disabled:

- MQTT disabled - no 'remote control'. Because of this, also no Home Assistant support!
- Debug Outputs via serial interface is disabled

General:

- 6 different sets of clock faces present. See [data](../data).
- BMP mode active for the given clock faces
- CORE_DEBUG_LEVEL=0 - No ESP32 core messages via serial interface.

## 2. Backup your original firmware

**Always backup YOUR clock firmware version as first step!**

On Windows, save your original firmware using `_ESP32 save flash 4MB.cmd` after changing the COM port to the number your clock uses.

On macOS/Linux, use the esptool bundled with PlatformIO:

`uv run python ~/.platformio/packages/tool-esptoolpy/esptool.py --port /dev/cu.usbserial-XXXX --baud 460800 read_flash 0 0x400000 backup1.bin`

Rename and store the `backup1.bin` on a save location.

See also the section "Backup first" and following in the `README.md` file in the root.

## 3. Available image file

| clock model | firmware image file |
| -- | -- |
| EleksTube IPS - Gen2 models (Classic Edition/Pro/PR1/PR2) | `FW_EleksTube_Gen2_v1.3.13.bin` |

## 4. Write the EleksTubeHAX firmware file

- Edit the file `_ESP32 write flash.cmd` with an editor.
- Write the correct COM port of your clock.
- Write the correct firmware file name.
- Run the CMD file.

The CMD file should look like this:

`esptool --port COM5 --baud 921600 write_flash --erase-all 0x0000 FW_EleksTube_Gen2_v1.3.13.bin`

Note: The clock will go into the "download mode" automatically when esptool is trying to write to it.

## 5. There is no warranty of any type

Use at your own risk!

If you mess-up your clock, it's only your fault!
