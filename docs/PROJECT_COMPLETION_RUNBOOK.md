# AcouSense Project Completion Runbook

This runbook reflects the current project state on May 5, 2026:

- The web UI is intended to run as a containerized application on a lightweight server or mini PC.
- The ESP32 remains the live data source and serves the operational API.
- The Android app includes the AI assistant flow, but the on-device model file is optional and can be added later.
- Your hardware is already powering on with a ready-made power module and a star-topology distribution, which is acceptable as long as all grounds remain common.

## 1. Software Components Now Wired for Deployment

### Web UI container
- Path: `C:\Users\stive\.codex\worktrees\79da\AcouSense\dashboard\frontend`
- Container file: `C:\Users\stive\.codex\worktrees\79da\AcouSense\dashboard\frontend\Dockerfile`
- Static server config: `C:\Users\stive\.codex\worktrees\79da\AcouSense\dashboard\frontend\nginx\default.conf`

### Auth API container
- Path: `C:\Users\stive\.codex\worktrees\79da\AcouSense\dashboard\auth-server`
- Container file: `C:\Users\stive\.codex\worktrees\79da\AcouSense\dashboard\auth-server\Dockerfile`

### Combined server deployment
- Compose file: `C:\Users\stive\.codex\worktrees\79da\AcouSense\dashboard\docker-compose.server.yml`
- Server env template: `C:\Users\stive\.codex\worktrees\79da\AcouSense\dashboard\.env.server.example`

### Frontend live data mode
- The web UI now accepts either:
  - the existing analytics backend shape (`/api/latest`, `/api/stats`), or
  - the ESP32-native API shape (`/api/status`, `/api/readings`, `/api/devices`)
- Relevant files:
  - `C:\Users\stive\.codex\worktrees\79da\AcouSense\dashboard\frontend\src\lib\sensorApi.ts`
  - `C:\Users\stive\.codex\worktrees\79da\AcouSense\dashboard\frontend\src\lib\acousenseApi.ts`
  - `C:\Users\stive\.codex\worktrees\79da\AcouSense\dashboard\frontend\src\pages\Devices.tsx`

## 2. Firmware and C++ Library Install List

Use these exactly in Arduino IDE Library Manager or PlatformIO.

### Main Arduino Nano firmware
- Firmware path: `C:\Users\stive\.codex\worktrees\79da\AcouSense\firmware\arduino`
- Sketch: `C:\Users\stive\.codex\worktrees\79da\AcouSense\firmware\arduino\acoustics_nano.ino`
- Libraries to install:

```text
LiquidCrystal_I2C
```

- Uses built-in core libraries as well:

```text
Wire
SPI
Arduino
```

### Main ESP32 firmware
- Firmware path: `C:\Users\stive\.codex\worktrees\79da\AcouSense\firmware\esp32`
- Sketch: `C:\Users\stive\.codex\worktrees\79da\AcouSense\firmware\esp32\acoustics_esp32.ino`
- Libraries to install:

```text
ESPAsyncWebServer
AsyncTCP
ArduinoJson
ESP32-sqlite3
```

- Uses ESP32 core libraries as well:

```text
WiFi
SPI
SD
BLE
Arduino
```

### Hardware validation sketches
- Path: `C:\Users\stive\.codex\worktrees\79da\AcouSense\firmware\tests`
- Sketch files:

```text
C:\Users\stive\.codex\worktrees\79da\AcouSense\firmware\tests\test_microphone.ino
C:\Users\stive\.codex\worktrees\79da\AcouSense\firmware\tests\test_lcd.ino
C:\Users\stive\.codex\worktrees\79da\AcouSense\firmware\tests\test_buzzer.ino
C:\Users\stive\.codex\worktrees\79da\AcouSense\firmware\tests\test_sdcard.ino
```

### Legacy/auxiliary PlatformIO ESP32 firmware
- Path: `C:\Users\stive\.codex\worktrees\79da\AcouSense\dashboard\dsp-noise-map\firmware`
- PlatformIO file: `C:\Users\stive\.codex\worktrees\79da\AcouSense\dashboard\dsp-noise-map\firmware\platformio.ini`
- `lib_deps` already declared there:

```text
kosme/arduinoFFT @ ^2.0.1
bblanchon/ArduinoJson @ ^6.21.3
knolleary/PubSubClient @ ^2.8
mikalhart/TinyGPSPlus @ ^1.0.3
```

## 3. Library Install Locations

If you use Arduino IDE on Windows, installed libraries normally end up here:

```text
C:\Users\<YOUR_WINDOWS_USERNAME>\Documents\Arduino\libraries\
```

Copy-paste example for your machine:

```text
C:\Users\stive\Documents\Arduino\libraries\
```

If you use PlatformIO, libraries are resolved automatically from:

```text
.pio\libdeps\<environment>\
```

For the legacy DSP firmware here, that becomes:

```text
C:\Users\stive\.codex\worktrees\79da\AcouSense\dashboard\dsp-noise-map\firmware\.pio\libdeps\esp32dev\
```

## 4. Hardware Note for Your Current Power Topology

Your ready-made power module plus star-shaped power distribution is fine for this project if you keep these rules:

1. Every module must still share one common electrical ground.
2. The ESP32 3.3V rail must remain isolated from direct 5V logic.
3. The Arduino-to-ESP32 SPI lines must still pass through the level shifter.
4. Keep the SD card module physically close to the ESP32 and keep its decoupling capacitors in place.
5. Check the 3.3V rail while the SD card is active, not only at idle.

## 5. Sequential Task List to Finish the Project

Run these in order.

1. Finalize the hardware wiring against `C:\Users\stive\.codex\worktrees\79da\AcouSense\docs\HARDWARE_BUILD_GUIDE.md`, keeping your star-power topology and common ground.
2. Confirm the logic level shifter is present on Arduino Nano SPI lines `D10`, `D11`, `D12`, and `D13` before connecting to ESP32 `GPIO5`, `GPIO23`, `GPIO19`, and `GPIO18`.
3. Verify the 5V rail under Arduino, LCD, buzzer, and microphone load.
4. Verify the 3.3V rail under ESP32 and SD-card load.
5. Run `C:\Users\stive\.codex\worktrees\79da\AcouSense\firmware\tests\test_lcd.ino` and record whether your LCD address is `0x27` or `0x3F`.
6. Update `LCD_I2C_ADDRESS` in `C:\Users\stive\.codex\worktrees\79da\AcouSense\firmware\arduino\acoustics_nano.ino` if needed.
7. Run `test_microphone.ino`, `test_buzzer.ino`, and `test_sdcard.ino` and confirm each module behaves correctly on its own.
8. Install the Arduino Nano libraries listed in section 2, then flash `C:\Users\stive\.codex\worktrees\79da\AcouSense\firmware\arduino\acoustics_nano.ino`.
9. Install the ESP32 libraries listed in section 2, set `WIFI_SSID` and `WIFI_PASSWORD` in `C:\Users\stive\.codex\worktrees\79da\AcouSense\firmware\esp32\acoustics_esp32.ino`, then flash it.
10. Boot the ESP32 and verify serial output shows SD mounted, database initialized, Wi-Fi connected, and web server started.
11. Copy `C:\Users\stive\.codex\worktrees\79da\AcouSense\dashboard\.env.server.example` to a real `.env` file next to `C:\Users\stive\.codex\worktrees\79da\AcouSense\dashboard\docker-compose.server.yml`.
12. Fill in `PUBLIC_WEB_ORIGIN`, `PUBLIC_AUTH_ORIGIN`, and `PUBLIC_ESP_API_ORIGIN` with the real server and ESP32 addresses.
13. Fill in `JWT_SECRET` and SMTP settings for production auth behavior.
14. On the server, go to `C:\Users\stive\.codex\worktrees\79da\AcouSense\dashboard` and run the compose deployment for the `webui` and `auth` containers.
15. Open the containerized web UI from a browser and confirm it can sign in through the auth container.
16. Confirm the web UI dashboard loads live readings from the ESP32 API.
17. Confirm the Devices page shows ESP32-known devices from `/api/devices`.
18. Build the Android app from `C:\Users\stive\.codex\worktrees\79da\AcouSense\mobile_app`.
19. Install the APK on a physical Android device, grant BLE permissions, and confirm BLE sync works with the ESP32.
20. Open the AI assistant screen in the mobile app and confirm the UI works even without a local model file.
21. If you want on-device AI later, place a model file on the phone and set its path in the app settings; otherwise leave AI in UI-only mode.
22. Run one end-to-end field test: create real sound events, verify Arduino sampling, ESP32 persistence, web UI visibility, BLE sync, and mobile history updates.
23. Export a sample dataset from the ESP32 API or web UI and keep it as your acceptance-test artifact.

## 6. Commands You Will Likely Use on the Server

From:

```text
C:\Users\stive\.codex\worktrees\79da\AcouSense\dashboard
```

Use:

```bash
docker compose --env-file .env -f docker-compose.server.yml build
docker compose --env-file .env -f docker-compose.server.yml up -d
docker compose --env-file .env -f docker-compose.server.yml logs -f
```

## 7. Important Current Limitation

I could prepare the deployment files and integration code, but I could not execute the actual Node, Python, or Android builds in this environment because the project dependencies are not installed locally in the workspace yet.
