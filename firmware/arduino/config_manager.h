/**
 * config_manager.h
 * AcouSense Arduino Firmware — Configuration Manager
 *
 * Stores runtime configuration received from the ESP32 via SPI.
 * Configuration lives in RAM only on the Arduino side — the ESP32
 * is the authoritative persistent store and re-sends config on boot.
 *
 * Spec: docs/specs/03_arduino_firmware_spec.md §3.5
 */

#pragma once
#include <stdint.h>

struct Config {
    uint16_t low_threshold;     // Normalized level (0-100) for low alert
    uint16_t medium_threshold;
    uint16_t high_threshold;
    uint8_t  pattern_low;       // Buzzer pattern ID for low severity
    uint8_t  pattern_medium;
    uint8_t  pattern_high;
};

class ConfigManager {
public:
    /** Initialize with firmware defaults. Call once in setup(). */
    static void init();

    /** Return the current configuration (const reference). */
    static const Config& get();

    /** Apply a new configuration received from the SPI packet. */
    static void apply(const Config& cfg);

private:
    static Config _config;
};
