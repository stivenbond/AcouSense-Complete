/**
 * config_manager.cpp
 * AcouSense Arduino Firmware — Configuration Manager
 */

#include "config_manager.h"

// ─── Static member definitions ───────────────────────────────────────────────

Config ConfigManager::_config;

// ─── Public API ──────────────────────────────────────────────────────────────

void ConfigManager::init() {
    // Firmware defaults — will be overwritten on first SPI config packet
    _config.low_threshold    = 40;
    _config.medium_threshold = 60;
    _config.high_threshold   = 80;
    _config.pattern_low      = 0x01;
    _config.pattern_medium   = 0x02;
    _config.pattern_high     = 0x03;
}

const Config& ConfigManager::get() {
    return _config;
}

void ConfigManager::apply(const Config& cfg) {
    _config = cfg;
}
