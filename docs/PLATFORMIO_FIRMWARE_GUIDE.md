# PlatformIO Firmware Guide

PlatformIO is the better choice for this project.

## Why I recommend PlatformIO

- You want to cycle through multiple firmwares and test sketches often.
- You want pinned, repeatable library management instead of per-machine Arduino IDE state.
- You have two target boards with different dependencies.
- You want command-based flashing so the workflow is easy to document and repeat.

For quick one-off experiments, Arduino IDE is still fine. For this repo as a project, PlatformIO is the cleaner long-term path.

## What is now configured

- Main config: `C:\Users\stive\.codex\worktrees\79da\AcouSense\platformio.ini`
- Shared-header sync script: `C:\Users\stive\.codex\worktrees\79da\AcouSense\tools\sync_shared_headers.py`

The sync script automatically copies shared contract headers into the firmware folders before each build so you do not have to do that manually.

## Environments

### Arduino Nano

- `nano-prod`
  - Flashes the production Arduino Nano firmware
  - Entry sketch: `firmware/arduino/acoustics_nano.ino`

- `nano-test-microphone`
  - Flashes `firmware/tests/test_microphone.ino`

- `nano-test-lcd`
  - Flashes `firmware/tests/test_lcd.ino`

- `nano-test-buzzer`
  - Flashes `firmware/tests/test_buzzer.ino`

### ESP32

- `esp32-prod`
  - Flashes the production ESP32 firmware
  - Entry sketch: `firmware/esp32/acoustics_esp32.ino`

- `esp32-test-sdcard`
  - Flashes `firmware/tests/test_sdcard.ino`

## Libraries now managed by PlatformIO

### `nano-prod` and `nano-test-lcd`

```text
johnrickman/LiquidCrystal_I2C@^1.1.4
```

### `esp32-prod`

```text
https://github.com/me-no-dev/AsyncTCP.git
https://github.com/me-no-dev/ESPAsyncWebServer.git
bblanchon/ArduinoJson@^7.3.1
https://github.com/siara-cc/esp32_arduino_sqlite3_lib.git
```

The rest comes from the selected board platform and Arduino framework.

## Commands

Run these from:

```text
C:\Users\stive\.codex\worktrees\79da\AcouSense
```

### Build only

```bash
pio run -e nano-prod
pio run -e esp32-prod
```

### Build and flash

```bash
pio run -e nano-prod -t upload
pio run -e esp32-prod -t upload
```

### Open serial monitor

```bash
pio device monitor -b 115200
```

### Flash validation sketches

```bash
pio run -e nano-test-lcd -t upload
pio run -e nano-test-microphone -t upload
pio run -e nano-test-buzzer -t upload
pio run -e esp32-test-sdcard -t upload
```

## Recommended flashing order

1. Flash `nano-test-lcd`
2. Record the LCD I2C address
3. Update `firmware/arduino/acoustics_nano.ino` if the address is not `0x27`
4. Flash `nano-test-microphone`
5. Flash `nano-test-buzzer`
6. Flash `esp32-test-sdcard`
7. Flash `nano-prod`
8. Edit Wi-Fi credentials in `firmware/esp32/acoustics_esp32.ino`
9. Flash `esp32-prod`

## What to flash and when

### During hardware validation

- Nano:
  - `nano-test-lcd`
  - `nano-test-microphone`
  - `nano-test-buzzer`

- ESP32:
  - `esp32-test-sdcard`

### During normal project use

- Nano:
  - `nano-prod`

- ESP32:
  - `esp32-prod`

## Arduino IDE vs PlatformIO in one sentence

If you want repeatability, versioned dependencies, less manual setup, and faster switching between firmwares, use PlatformIO for this repo and keep Arduino IDE only as an emergency fallback.
