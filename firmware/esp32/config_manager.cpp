/**
 * config_manager.cpp
 * AcouSense ESP32 Firmware — Configuration Manager
 */

#include "config_manager.h"
#include "database_manager.h"
#include "spi_master_manager.h"
#include <Arduino.h>

ESPConfig ESP32ConfigManager::_config = {};
bool      ESP32ConfigManager::_loaded = false;

void ESP32ConfigManager::init() {
    DBConfig dbcfg;
    if (DatabaseManager::getConfig(dbcfg)) {
        _config.low_threshold    = (uint16_t)dbcfg.low_threshold;
        _config.medium_threshold = (uint16_t)dbcfg.medium_threshold;
        _config.high_threshold   = (uint16_t)dbcfg.high_threshold;
        _config.pattern_low      = (uint8_t)dbcfg.pattern_low;
        _config.pattern_medium   = (uint8_t)dbcfg.pattern_medium;
        _config.pattern_high     = (uint8_t)dbcfg.pattern_high;
        _loaded = true;
        Serial.println(F("[Config] Loaded from DB"));
    } else {
        // Fallback to firmware defaults
        _config = { 40, 60, 80, 1, 2, 3 };
        _loaded = false;
        Serial.println(F("[Config] DB load failed — using defaults"));
    }
}

const ESPConfig& ESP32ConfigManager::get() { return _config; }
bool             ESP32ConfigManager::isLoaded() { return _loaded; }

bool ESP32ConfigManager::apply(const ESPConfig& cfg) {
    _config = cfg;

    // Persist to DB
    DBConfig dbcfg;
    dbcfg.low_threshold    = cfg.low_threshold;
    dbcfg.medium_threshold = cfg.medium_threshold;
    dbcfg.high_threshold   = cfg.high_threshold;
    dbcfg.pattern_low      = cfg.pattern_low;
    dbcfg.pattern_medium   = cfg.pattern_medium;
    dbcfg.pattern_high     = cfg.pattern_high;

    bool saved = DatabaseManager::saveConfig(dbcfg);
    if (!saved) {
        Serial.println(F("[Config] Warning: DB save failed"));
    }

    // Push new config to Arduino via SPI
    SPIMasterManager::pushConfig(cfg);

    return saved;
}
