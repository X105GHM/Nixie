# Disclaimer

This project documentation was prepared with the help of **ChatGPT 5.1**.

# Nixie Clock V6 Project Description

## Overview

**Nixie Clock V6** is an ESP32-S3-based Nixie clock firmware + web interface project for controlling and monitoring a Nixie tube clock system (HV5122-based high-voltage digit driving).

The project combines:

- real-time clock display control
- a browser-based configuration dashboard
- OTA firmware updates
- persistent settings storage
- telemetry (temperature, voltages, power values)
- alarm/timer and automation-style features

It is written primarily in **C++** and built with **PlatformIO**, with a web frontend served from **SPIFFS**.

## Project Photos (V6 Clock)

These photos show the assembled **Nixie V6** clock hardware.

<p>
  <img src="https://raw.githubusercontent.com/X105GHM/Nixie/pics/V6_Nixie/Nixie1.png" alt="Nixie V6 Clock Photo 1" width="32%">
  <img src="https://raw.githubusercontent.com/X105GHM/Nixie/pics/V6_Nixie/Nixie2.png" alt="Nixie V6 Clock Photo 2" width="32%">
  <img src="https://raw.githubusercontent.com/X105GHM/Nixie/pics/V6_Nixie/Nixie3.png" alt="Nixie V6 Clock Photo 3" width="32%">
</p>

## Main Features

### Clock and Display Control

- Nixie digit display task running on FreeRTOS
- Display enable/disable control
- PWM-based brightness control
- Manual brightness mode
- Time-limited display mode (active only between configured times)
- Silent mode support

### Clock Utility Features

- Timer (configurable duration)
- Alarm (configurable alarm time)
- Date display trigger
- Anti-cathode-poisoning (ACP) routines
- Selective tube cleaning (individual digit/cathode cleaning)

### Connectivity and Time/Weather

- Wi-Fi connection management
- NTP-based time synchronization with timezone support
- ZIP code + weather update integration
- Web UI Wi-Fi manager with priority ordering (drag & drop)
- Multi-network management (add/remove/reorder saved Wi-Fi networks)

### Monitoring and Diagnostics

- Dashboard with system/device information (chip, SDK, flash, heap, firmware target, etc.)
- Voltage/temperature/current/power/energy telemetry fields
- Live graph section with selectable time windows
- Log tab with configurable log categories (HTTP, TIME, HSS, DIGIT, OTA, WIFI, etc.)

### OTA and Build Packaging

- OTA firmware update support from the web interface
- Build script generates:
  - `firmware.bin`
  - `spiffs.bin`
  - `manifest.json` (with SHA-256 checksums + timestamp)
  - timestamped OTA ZIP package (`ota_package_YYYYMMDD_HHMM.zip`)
- Software version injection during build via PlatformIO extra script

### V6.7 ESP-IDF Release / GitHub OTA

`bin/version.txt` is the release version (starting with `Nixie_V.6.7.0`).
Set it explicitly before a new release; builds do not increment it. The same
value is compiled into the UI and the ESP-IDF application descriptor, and the
packager checks it before writing `manifest.json`.

Run `pio run` (or `pio run -t package_ota` / `pio run -t build_bins`) to build the
firmware and SPIFFS and generate the complete OTA package in `bin/`. Firmware
deliberately includes the OpenWeather API key from `src/Config/LocalSecrets.hpp`
when configured; that source file remains ignored by Git.

Publish `bin/firmware.bin`, `bin/spiffs.bin`, `bin/manifest.json` and
`bin/version.txt` together in the GitHub branch selected on the clock:
`V.6` for `NixieV6_std`, or `V.6_dev` for `NixieV6_dev`. The updater reads these
raw branch files, not GitHub Release attachments or the ZIP. Both binaries are
needed because the IDF firmware uses the updated SPIFFS web interface.

The V6.6.x updater detects V6.7.0 as a different version. Update checks continue
to allow switching to a different version in another channel, including an older
one. The partition layout is unchanged from the last Arduino build. Validate the
first migration on a V6.6.x clock with its existing bootloader before wider rollout.

V6.7.1 reserves the OTA worker's 18 KB stack and task control block in internal
RAM. The worker waits for notifications between updates, so starting OTA does
not require a large contiguous heap allocation. The previous display state is
restored after success or failure. `/api/v1/ota/status` also reports free internal
heap, its largest free block and the OTA stack high-water mark (in bytes).
Run `python test/ota_lifecycle_test.py` for host checks of start failures, busy
requests, repeated updates and display/SPIFFS cleanup with a simulated transport.

## Firmware Architecture (High-Level)

The firmware is organized into modules and services (for example: clock control, digits, Wi-Fi, HTTP, OTA, temperature, memory, brownout handling, and supply monitoring).

A typical startup flow (as implemented in the project) follows this structure:

1. Initialize native ESP-IDF logging
2. Initialize/check PSRAM
3. Initialize persistent storage and load saved settings
4. Connect to Wi-Fi
5. Initialize NTP time with timezone
6. Perform load detection / high-voltage supply checks
7. Start dedicated tasks:
   - Clock task
   - Button polling task
   - Brownout handling task
   - HTTP/web task
   - Display digits task

This structured FreeRTOS-based design keeps the project modular and responsive, while separating timing-critical display handling from web and control logic.

## Web Interface (SPIFFS Frontend)

The frontend is stored in the `data/` folder and served by the ESP32.

The interface includes the following main sections:

- **Dashboard**
- **Graphs**
- **Log**
- **Settings**
- **Info**

The UI exposes many clock and device controls, including:

- display on/off
- ACP routine trigger
- date/weather display trigger
- silent mode
- ticker enable
- time window limits
- Nixie PWM + manual brightness
- weather updates
- random “cricket” sound option
- timer/alarm toggles and time settings
- ZIP code and timezone settings
- log configuration flags
- firmware target selection (`NixieV6_std`, `NixieV6_dev`, `NixieV6_BOS`)
- OTA update trigger
- Wi-Fi manager with priority sorting and deletion

The frontend polls `/get/info` repeatedly and updates dashboard values and graphs in near real time.

### GUI Screenshots

#### Dashboard / Themes

<p>
  <img src="https://raw.githubusercontent.com/X105GHM/Nixie/pics/GUI_Nixie/Dashboard_Dark.png" alt="Dashboard Dark Theme" width="49%">
  <img src="https://raw.githubusercontent.com/X105GHM/Nixie/pics/GUI_Nixie/Dashboard_White.png" alt="Dashboard Light Theme" width="49%">
</p>

#### Charts / Telemetry

<img src="https://raw.githubusercontent.com/X105GHM/Nixie/pics/GUI_Nixie/Charts.png" alt="Charts View" width="900">

#### Log / Info

<p>
  <img src="https://raw.githubusercontent.com/X105GHM/Nixie/pics/GUI_Nixie/Log.png" alt="Log View" width="49%">
  <img src="https://raw.githubusercontent.com/X105GHM/Nixie/pics/GUI_Nixie/Info.png" alt="Info View" width="49%">
</p>

#### Settings

<p>
  <img src="https://raw.githubusercontent.com/X105GHM/Nixie/pics/GUI_Nixie/Settings_1.png" alt="Settings View 1" width="49%">
  <img src="https://raw.githubusercontent.com/X105GHM/Nixie/pics/GUI_Nixie/Settings_2.png" alt="Settings View 2" width="49%">
</p>

#### Mobile Layout

<img src="https://raw.githubusercontent.com/X105GHM/Nixie/pics/GUI_Nixie/Settings_mobile.png" alt="Mobile Settings View" width="420">

## Hardware / PCB

The project includes a dedicated Nixie V6 PCB. The images below show the front and back side of the board.

<p>
  <img src="https://raw.githubusercontent.com/X105GHM/Nixie/pics/PCB_Nixie/PCB_Nixie_Front.png" alt="Nixie PCB Front" width="49%">
  <img src="https://raw.githubusercontent.com/X105GHM/Nixie/pics/PCB_Nixie/PCB_Nixie_Back.png" alt="Nixie PCB Back" width="49%">
</p>

## 3D Printable Case

A printable enclosure for the Nixie V6 project is available on Printables:

- [Nixie V6 Case (Printables)](https://www.printables.com/model/1354364-nixie-v6-case)

## Build Environment

The project is configured for an **ESP32-S3** target in PlatformIO:

- **Framework:** ESP-IDF 5.5.4
- **Platform:** `pioarduino/platform-espressif32@55.3.38`
- **Board env:** `esp32-s3-wroom-1-n16r8`
- **Filesystem:** SPIFFS
- **C++ standard:** GNU++17
- **Flash / PSRAM setup:** 16 MB flash + PSRAM configuration for S3 module

A custom partition table is included with:

- NVS
- OTA data
- two OTA app slots (`app0`, `app1`)
- SPIFFS partition
- coredump partition

This is well-suited for reliable OTA updates and a sizeable web frontend.

## Repository Structure (Top-Level)

The repository includes (among others):

- `src/` – firmware source code
- `data/` – web UI files served from SPIFFS
- `include/` – shared headers
- `scripts/` – build / OTA helper scripts
- `lib/CustomWiFiManager/` – custom Wi-Fi management components
- `test/` – test-related files
- `platformio.ini` – PlatformIO project config
- `partitions.csv` – custom ESP32 partition layout

## Summary

This repository is a complete embedded + web stack for a feature-rich **Nixie Clock**, built around an **ESP32-S3** and designed for:

- classic Nixie clock control
- modern browser-based configuration
- telemetry and diagnostics
- OTA firmware maintenance
- persistent user settings

