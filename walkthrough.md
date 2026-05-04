# AcouSense — Project Walkthrough

> Full implementation walkthrough for the AcouSense Noise Exposure Monitoring Ecosystem.
> All 8 phases complete. 92 source files across 4 subsystems.

---

## Architecture Overview

```
┌────────────────────────────────────────────────────────────────┐
│  Arduino Nano (Sensor Hub)     ESP32-D2 (Gateway)              │
│  ─────────────────────────     ──────────────────────────────  │
│  Microphone (A0)              SD Card (SQLite DB)              │
│  LCD 16x2 I2C                 Wi-Fi (REST API / Dashboard)     │
│  Buzzer (PWM D9)              Bluetooth LE (GATT Central)      │
│  SPI Slave ←───LLS────────►  SPI Master                       │
└────────────────────────────────────────────────────────────────┘
                                         │ BLE GATT
                                         ▼
                              ┌──────────────────────┐
                              │  Android App          │
                              │  BLE GATT Server      │
                              │  Room DB              │
                              │  Gemma 2B (on-device) │
                              └──────────────────────┘
                                         │
                              Wi-Fi (Browser)
                                         │
                              ┌──────────────────────┐
                              │  SvelteKit Dashboard  │
                              │  served from ESP32 SD │
                              └──────────────────────┘
```

---

## Layer-by-Layer Summary

### Shared Contracts (`shared/`)
The single source of truth — copied into both firmware trees before flashing.

| File | Role |
|---|---|
| `packet_protocol.h` | Binary SPI framing, CRC8 (Dallas `0x31`), payload structs |
| `spi_constants.h` | All pin assignments for both SPI buses |
| `ble_constants.h` | BLE UUIDs + 52-byte `SyncPayload` struct layout |
| `exposure_classes.h` | WHO-based 4-tier classification (None/Low/Moderate/High) |

---

### Hardware Validation (`firmware/tests/`)
Run each sketch independently after assembly. Validates one peripheral at a time before integration.

| Sketch | What it verifies |
|---|---|
| `test_microphone.ino` | 20 Hz ADC, rolling min/max/avg to Serial |
| `test_lcd.ino` | I2C address scan + 4 rotating display states |
| `test_buzzer.ino` | All 4 patterns in sequence |
| `test_sdcard.ino` | Mount → mkdir → write → read → match → delete → report |

---

### Arduino Nano Firmware (`firmware/arduino/`)

**Core rule: `delay()` is banned. All scheduling uses `millis()`.**

| Module | Mechanism |
|---|---|
| `SensorManager` | 20 Hz ADC via `millis()` gate; 10-second rolling window → `AudioReportPayload` |
| `LCDManager` | 500 ms refresh gate; shows level / alert / ESP32 connection state |
| `BuzzerManager` | Elapsed-time patterns: silent / single-beep / double-pulse / rapid-alarm |
| `SPISlaveManager` | `SPI_STC_vect` ISR handles byte I/O; SS rising-edge detection triggers packet processing |
| `ConfigManager` | RAM-only config overwritten when ESP32 pushes a `0x02 Config` packet |

**SPI transaction model:** Arduino pre-stages its TX response before CS assertion. The ISR clock-shifts TX out while simultaneously clocking RX in. After CS de-asserts, the main loop processes the received packet and pre-stages the next response.

---

### ESP32 Firmware (`firmware/esp32/`)

**Boot order is strictly dependency-ordered** (see `acoustics_esp32.ino` comments).

| Module | Key behaviour |
|---|---|
| `SDCardManager` | Separate SPI bus; creates `/acousense/` and `/www/` dirs on first boot |
| `DatabaseManager` | WAL mode SQLite; prepared statements throughout; `ON CONFLICT DO UPDATE` for BT upsert |
| `ESP32ConfigManager` | Persists to DB on `apply()`; immediately calls `SPIMasterManager::pushConfig()` |
| `SPIMasterManager` | Polls Arduino every 10s; 3-retry ACK for config push; writes received reports to DB |
| `AcouWebServer` | ESPAsyncWebServer + ArduinoJson v7; 6 REST endpoints + SPA static fallback |
| `BluetoothManager` | BLE Central continuous scan; GATT client connection for `deliverSync()` |
| `SyncManager` | 5-min aggregation → per-device `SyncPayload` delivery; resets presence table after each cycle |

---

### Web Dashboard (`dashboard/`)

SvelteKit + `@sveltejs/adapter-static` → pure SPA, zero SSR.

| Route | Features |
|---|---|
| `/` | SVG arc gauge (colour-shifts with alert level), 5s auto-poll, sparkline, health panel |
| `/history` | Date-range presets, Chart.js line chart (min/max toggle), paginated table, CSV export |
| `/config` | Threshold sliders + live colour-band bar, pattern selects, client-side validation |
| `/devices` | Presence cards with animated glow bar, stats overview, full device table |

**Design:** `#0B0D14` dark background · `#6C63FF` accent · Inter + JetBrains Mono · glassmorphism cards · animated alert badges.

**Deploy steps:**
```bash
cd dashboard
npm install
npm run build           # outputs to dashboard/build/
# Copy build/ contents to SD card /www/
```

---

### Android App (`android/`)

Kotlin • Jetpack Compose • Hilt DI • Room • WorkManager • MediaPipe LLM.

| Screen | Feature |
|---|---|
| Dashboard | Canvas arc gauge, session list, shimmer loading |
| History | Range chips (7/30/all), exposure distribution bar, daily cards with AI preview |
| Recommendations | Per-day Gemma generation with streaming typewriter effect |
| Settings | User ID, Gemma model path, retention slider, about panel |

**BLE role:** Android is the **GATT Peripheral** (server). ESP32 is the **GATT Central** (client that connects and writes). This is the reverse of the typical smartphone-as-central pattern.

**Gemma model setup:**
```bash
# Accept license at huggingface.co/google/gemma-2b-it
# Then:
adb push gemma-2b-it-cpu-int4.bin /data/local/tmp/acousense/gemma.bin
```

---

## Action Checklist (Hardware → Software)

```
Phase 1 — Hardware
  [ ] Wire per docs/HARDWARE_BUILD_GUIDE.md
  [ ] Install logic level shifter on all 4 SPI lines
  [ ] Install 100µF decoupling cap on SD card Vcc
  [ ] Run test_microphone.ino → verify Serial output
  [ ] Run test_lcd.ino → scan I2C address, update LCD_I2C_ADDRESS in acoustics_nano.ino
  [ ] Run test_buzzer.ino → verify all 4 patterns
  [ ] Run test_sdcard.ino → verify R/W cycle

Phase 2 — Arduino Flash
  [ ] Copy shared/ headers into firmware/arduino/
  [ ] Install LiquidCrystal_I2C library
  [ ] Upload acoustics_nano.ino
  [ ] Verify LCD shows "AcouSense v1.0" then live data

Phase 3 — ESP32 Flash
  [ ] Copy shared/ headers into firmware/esp32/
  [ ] Set WIFI_SSID / WIFI_PASSWORD in acoustics_esp32.ino
  [ ] Install ESPAsyncWebServer, AsyncTCP, ArduinoJson v7, ESP32-sqlite3 libraries
  [ ] Upload acoustics_esp32.ino
  [ ] Verify Serial shows SD ✓, DB ✓, SPI config pushed, WiFi connected

Phase 4 — Web Dashboard
  [ ] cd dashboard && npm install && npm run build
  [ ] Copy build/ to SD card /www/
  [ ] Navigate to http://<ESP32_IP> in browser

Phase 5 — Android App
  [ ] Open android/ in Android Studio
  [ ] Sync Gradle — all deps resolve automatically
  [ ] Push Gemma model with adb
  [ ] Build & run on device (minSdk 31 = Android 12)
  [ ] Grant BLE permissions when prompted
  [ ] Observe BLE GATT service notification in status bar
```

---

## Required External Libraries

### ESP32 Arduino IDE Libraries
| Library | Source |
|---|---|
| `ESPAsyncWebServer` | Library Manager — me-no-dev |
| `AsyncTCP` | Library Manager — me-no-dev |
| `ArduinoJson` v7 | Library Manager — Benoit Blanchon |
| `ESP32-sqlite3` | Library Manager — Siara-cc |

### Arduino Nano Libraries
| Library | Source |
|---|---|
| `LiquidCrystal_I2C` | Library Manager — Frank de Brabander |

### Android
All declared in `android/gradle/libs.versions.toml` — resolved by Gradle automatically.

---

*Generated by AcouSense implementation assistant — April 2026*
