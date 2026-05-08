/**
 * acoustics_esp32.cpp
 * AcouSense — ESP32-D2 Firmware Entry Point
 *
 * System coordinator. Initializes all subsystems in dependency order,
 * then drives a non-blocking cooperative loop.
 *
 * Boot sequence:
 *   1. Serial (debug)
 *   2. SDCardManager   — SD mount, directory init
 *   3. DatabaseManager — SQLite open, schema, default config
 *   4. ESP32ConfigManager — load config from DB
 *   5. SPIMasterManager — SPI bus init, push config to Arduino
 *   6. Wi-Fi connection
 *   7. NTP time sync (optional — falls back to millis if unavailable)
 *   8. AcouWebServer   — start HTTP server on port 80
 *   9. BluetoothManager — BLE init, start scan
 *  10. SyncManager     — reset 5-minute cycle timer
 *
 * Main loop:
 *   All module updates are non-blocking (millis() based).
 *   ESPAsyncWebServer handles HTTP requests on its own RTOS task.
 *
 * Required libraries (install via Library Manager):
 *   - ESPAsyncWebServer (me-no-dev)
 *   - AsyncTCP (me-no-dev)
 *   - ArduinoJson v7 (Benoit Blanchon)
 *   - ESP32-sqlite3 (Siara-cc)
 *   (BLE, WiFi, SPI, SD — included with ESP32 Arduino Core)
 *
 * Spec: docs/specs/04_esp32_firmware_spec.md
 */

#include "sd_card_manager.h"
#include "database_manager.h"
#include "config_manager.h"
#include "spi_master_manager.h"
#include "web_server.h"
#include "bluetooth_manager.h"
#include "sync_manager.h"

#include <WiFi.h>
#include <Arduino.h>
#include <string.h>

// ─── Wi-Fi credentials ────────────────────────────────────────────────────────
#if __has_include("credentials.h")
#include "credentials.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID     ""
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

#ifndef WIFI_AP_SSID
#define WIFI_AP_SSID  "AcouSense-ESP32"
#endif

#ifndef WIFI_AP_PASSWORD
#define WIFI_AP_PASSWORD "acousense"
#endif

#define WIFI_TIMEOUT_MS  20000UL  // 20 seconds max connection wait

// ─── NTP ─────────────────────────────────────────────────────────────────────
#define NTP_SERVER  "pool.ntp.org"
#define NTP_TZ      "UTC0"          // Change to local timezone if desired

// ─── Wi-Fi Helper ────────────────────────────────────────────────────────────

static bool connectWiFi() {
    if (strlen(WIFI_SSID) == 0 || strcmp(WIFI_SSID, "YOUR_SSID") == 0) {
        Serial.println(F("[WiFi] No station credentials configured"));
        return false;
    }

    Serial.print(F("[WiFi] Connecting to "));
    Serial.println(WIFI_SSID);
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_TIMEOUT_MS) {
            Serial.println(F("\n[WiFi] Connection timed out"));
            return false;
        }
        delay(250);
        Serial.print('.');
    }
    Serial.println();
    Serial.print(F("[WiFi] Connected — IP: "));
    Serial.println(WiFi.localIP());
    return true;
}

static bool startAccessPoint() {
    WiFi.mode(WIFI_AP_STA);
    bool ok = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
    if (!ok) {
        Serial.println(F("[WiFi] AP start failed"));
        return false;
    }

    Serial.print(F("[WiFi] AP started — SSID: "));
    Serial.println(WIFI_AP_SSID);
    Serial.print(F("[WiFi] AP IP: "));
    Serial.println(WiFi.softAPIP());
    return true;
}

// ─── Setup ───────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println(F("\n============================"));
    Serial.println(F("  AcouSense ESP32 Firmware  "));
    Serial.println(F("============================"));

    // 1. SD Card
    SDCardManager::init();

    // 2. Database (requires SD)
    if (SDCardManager::isMounted()) {
        DatabaseManager::init();
    } else {
        Serial.println(F("[Boot] SD failed — DB and web serving disabled"));
    }

    // 3. Config (requires DB)
    if (DatabaseManager::isReady()) {
        ESP32ConfigManager::init();
    }

    // 4. SPI Master (requires Config)
    SPIMasterManager::init();

    // 5. Wi-Fi
    // Always expose a local setup/dashboard network. Station mode is optional.
    bool apOk = startAccessPoint();
    bool staOk = connectWiFi();
    bool wifiOk = apOk || staOk;

    // 6. NTP time sync (best-effort — not critical)
    if (staOk) {
        configTzTime(NTP_TZ, NTP_SERVER);
        Serial.println(F("[NTP] Time sync requested"));
        delay(1000);  // Brief wait for initial NTP reply
    } else {
        Serial.println(F("[NTP] Skipped (station WiFi not connected)"));
    }

    // 7. Web Server (requires Wi-Fi + DB)
    if (wifiOk && DatabaseManager::isReady()) {
        AcouWebServer::init();
    } else {
        Serial.println(F("[Boot] Web server skipped (no WiFi or DB)"));
    }

    // 8. Bluetooth (Temporarily disabled for Power/Stability testing)
    BluetoothManager::init();

    // 9. Sync Manager (Depends on BT, but safe to init - will just idle)
    SyncManager::init();

    Serial.println(F("[Boot] System ready\n"));
}

// ─── Loop ────────────────────────────────────────────────────────────────────

void loop() {
    // All modules are non-blocking — update unconditionally
    SPIMasterManager::update();    // 10s SPI poll cycle
    BluetoothManager::update();    // Continuous BLE scan management
    SyncManager::update();         // 5-minute sync cycle
    AcouWebServer::update();       // No-op (async server is self-driven)

    // Yield to RTOS for async web server tasks
    yield();
}
