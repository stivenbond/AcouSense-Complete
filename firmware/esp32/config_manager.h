/**
 * config_manager.h
 * AcouSense ESP32 Firmware — Configuration Manager
 *
 * Loads configuration from SQLite on boot, caches it in RAM, and
 * exposes it to SPI, Web, and Sync modules. Persists changes to DB.
 *
 * Spec: docs/specs/04_esp32_firmware_spec.md §3.4
 */

#pragma once
#include <stdint.h>

struct ESPConfig {
    uint16_t low_threshold;
    uint16_t medium_threshold;
    uint16_t high_threshold;
    uint8_t  pattern_low;
    uint8_t  pattern_medium;
    uint8_t  pattern_high;
};

class ESP32ConfigManager {
public:
    /**
     * Load config from DB (via DatabaseManager).
     * Must be called after DatabaseManager::init() succeeds.
     */
    static void init();

    /** Return the in-memory config. */
    static const ESPConfig& get();

    /**
     * Apply and persist a new configuration.
     * Saves to DB and signals SPIMasterManager to push 0x02 to Arduino.
     * Returns true on success.
     */
    static bool apply(const ESPConfig& cfg);

    /** True if config was successfully loaded from DB. */
    static bool isLoaded();

private:
    static ESPConfig _config;
    static bool      _loaded;
};
