# AcouSense — Project Structure

## Root

```
AcouSense/
├── PROJECT_BRIEF.md                  # Original system specification
├── PROJECT_STRUCTURE.md              # This file
│
├── shared/                           # Language-agnostic constants & contracts
│   ├── packet_protocol.h             # SPI packet structs, CRC8, type constants
│   ├── spi_constants.h               # Pin assignments, clock speed, timing
│   ├── ble_constants.h               # BLE service & characteristic UUIDs
│   └── exposure_classes.h            # WHO-based exposure classification thresholds
│
├── firmware/
│   ├── arduino/                      # Arduino Nano V3 firmware
│   │   ├── acoustics_nano.cpp        # Entry point (setup + loop)
│   │   ├── sensor_manager.h
│   │   ├── sensor_manager.cpp
│   │   ├── lcd_manager.h
│   │   ├── lcd_manager.cpp
│   │   ├── buzzer_manager.h
│   │   ├── buzzer_manager.cpp
│   │   ├── spi_slave_manager.h
│   │   ├── spi_slave_manager.cpp
│   │   ├── config_manager.h
│   │   ├── config_manager.cpp
│   │   └── packet_protocol.h         # Copied from shared/
│   │
│   ├── esp32/                        # ESP32-D2 firmware
│   │   ├── acoustics_esp32.cpp       # Entry point (setup + loop)
│   │   ├── spi_master_manager.h
│   │   ├── spi_master_manager.cpp
│   │   ├── sd_card_manager.h
│   │   ├── sd_card_manager.cpp
│   │   ├── database_manager.h
│   │   ├── database_manager.cpp
│   │   ├── config_manager.h
│   │   ├── config_manager.cpp
│   │   ├── web_server.h
│   │   ├── web_server.cpp
│   │   ├── bluetooth_manager.h
│   │   ├── bluetooth_manager.cpp
│   │   ├── sync_manager.h
│   │   ├── sync_manager.cpp
│   │   └── packet_protocol.h         # Copied from shared/
│   │
│   └── tests/                        # Standalone hardware validation sketches (Phase 1)
│       ├── test_microphone.cpp
│       ├── test_lcd.cpp
│       ├── test_buzzer.cpp
│       └── test_sdcard.cpp
│
├── dashboard/                        # SvelteKit web dashboard
│   ├── src/
│   │   ├── routes/
│   │   │   ├── +layout.svelte        # Global nav + design shell
│   │   │   ├── +page.svelte          # Live Dashboard (/)
│   │   │   ├── history/
│   │   │   │   └── +page.svelte      # Historical Data Viewer
│   │   │   ├── config/
│   │   │   │   └── +page.svelte      # Configuration Panel
│   │   │   └── devices/
│   │   │       └── +page.svelte      # Device Management
│   │   ├── lib/
│   │   │   ├── components/
│   │   │   │   ├── StatusCard.svelte
│   │   │   │   ├── AlertBadge.svelte
│   │   │   │   ├── NoiseGauge.svelte
│   │   │   │   ├── SparklineChart.svelte
│   │   │   │   └── DataTable.svelte
│   │   │   ├── api.ts                # Typed fetch wrappers for all endpoints
│   │   │   └── types.ts              # TypeScript interfaces
│   │   └── app.css                   # Global design system tokens
│   ├── static/
│   ├── svelte.config.js
│   ├── vite.config.ts
│   └── package.json
│
├── android/                          # Android application (Kotlin + Compose)
│   ├── app/
│   │   ├── src/main/
│   │   │   ├── java/com/acousense/
│   │   │   │   ├── MainActivity.kt
│   │   │   │   ├── ui/
│   │   │   │   │   ├── dashboard/
│   │   │   │   │   │   ├── DashboardScreen.kt
│   │   │   │   │   │   └── DashboardViewModel.kt
│   │   │   │   │   ├── history/
│   │   │   │   │   │   ├── HistoryScreen.kt
│   │   │   │   │   │   └── HistoryViewModel.kt
│   │   │   │   │   ├── recommendations/
│   │   │   │   │   │   ├── RecommendationsScreen.kt
│   │   │   │   │   │   └── RecommendationsViewModel.kt
│   │   │   │   │   └── settings/
│   │   │   │   │       ├── SettingsScreen.kt
│   │   │   │   │       └── SettingsViewModel.kt
│   │   │   │   ├── data/
│   │   │   │   │   ├── db/
│   │   │   │   │   │   ├── AppDatabase.kt
│   │   │   │   │   │   ├── SyncSessionDao.kt
│   │   │   │   │   │   ├── DailySummaryDao.kt
│   │   │   │   │   │   ├── SyncSession.kt  (entity)
│   │   │   │   │   │   └── DailySummary.kt (entity)
│   │   │   │   │   ├── ble/
│   │   │   │   │   │   ├── BleGattService.kt
│   │   │   │   │   │   ├── BleConstants.kt
│   │   │   │   │   │   └── SyncPayloadParser.kt
│   │   │   │   │   └── ai/
│   │   │   │   │       ├── GemmaInference.kt
│   │   │   │   │       └── PromptBuilder.kt
│   │   │   │   ├── domain/
│   │   │   │   │   ├── ExposureRepository.kt
│   │   │   │   │   ├── BleRepository.kt
│   │   │   │   │   └── AiRepository.kt
│   │   │   │   └── di/
│   │   │   │       └── AppModule.kt
│   │   │   ├── res/
│   │   │   └── AndroidManifest.xml
│   │   └── build.gradle.kts
│   ├── build.gradle.kts
│   └── settings.gradle.kts
│
└── docs/
    ├── IMPLEMENTATION_PLAN.md        # Sequential implementation phases
    ├── specs/
    │   ├── 01_hardware_and_schematic_spec.md
    │   ├── 02_spi_communication_spec.md
    │   ├── 03_arduino_firmware_spec.md
    │   ├── 04_esp32_firmware_spec.md
    │   ├── 05_database_spec.md
    │   ├── 06_web_dashboard_spec.md
    │   ├── 07_bluetooth_sync_spec.md
    │   └── 08_android_app_spec.md
    └── diagrams/                     # Wiring diagrams, architecture diagrams (future)
```

---

## Notes

- `shared/` contains the ground truth for all inter-system contracts. Any change to packet structures, UUIDs, or thresholds must originate here.
- `packet_protocol.h` is duplicated into `firmware/arduino/` and `firmware/esp32/` because the Arduino IDE does not support referencing parent directory headers. Treat these as copies that must be kept in sync with `shared/`.
- `firmware/tests/` sketches are disposable validation tools (Phase 1 only). They are not part of the production firmware.
- `dashboard/` is a standard SvelteKit project. The production output (`npm run build`) is served from the SD card's `/www/` directory by the ESP32.
- `android/` is a standard Android Studio Gradle project using the version catalog convention.
