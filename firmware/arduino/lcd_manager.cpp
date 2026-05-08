/**
 * lcd_manager.cpp
 * AcouSense Arduino Firmware — LCD Manager
 */

#include "lcd_manager.h"
#include "sensor_manager.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Arduino.h>

#define LCD_REFRESH_MS  500UL  // Refresh every 500ms

// Module-level LCD instance (initialized in init())
static LiquidCrystal_I2C* _lcd = nullptr;

// ─── Static member definitions ───────────────────────────────────────────────

uint8_t         LCDManager::_address        = 0x27;
unsigned long   LCDManager::_lastRefreshTime = 0;
ConnectionState LCDManager::_connState      = ConnectionState::UNKNOWN;

// ─── Private helpers ─────────────────────────────────────────────────────────

void LCDManager::_render(uint16_t level, uint8_t alertLevel) {
    if (!_lcd) return;

    // ── Line 1: Level ─────────────────────────────────────────────────────
    // Format: "Level: 078/100  "
    _lcd->setCursor(0, 0);
    _lcd->print(F("Level: "));

    // Zero-pad to 3 digits
    if (level < 10)       _lcd->print(F("00"));
    else if (level < 100) _lcd->print(F("0"));
    _lcd->print(level);
    _lcd->print(F("/100  "));

    // ── Line 2: Alert + Connection ────────────────────────────────────────
    // Format: "ALT:HIGH  CONN  " (16 chars exactly)
    _lcd->setCursor(0, 1);
    _lcd->print(F("ALT:"));

    switch (alertLevel) {
        case 0: _lcd->print(F("NONE")); break;
        case 1: _lcd->print(F("LOW ")); break;
        case 2: _lcd->print(F("MED ")); break;
        case 3: _lcd->print(F("HIGH")); break;
        default: _lcd->print(F("??? ")); break;
    }

    _lcd->print(F("  "));

    switch (_connState) {
        case ConnectionState::UNKNOWN:   _lcd->print(F("---")); break;
        case ConnectionState::CONNECTED: _lcd->print(F(" OK")); break;
        case ConnectionState::OFFLINE:   _lcd->print(F("ERR")); break;
    }

    _lcd->print(F("   "));  // Pad to end of line
}

// ─── Public API ──────────────────────────────────────────────────────────────

void LCDManager::init(uint8_t i2cAddress) {
    _address = i2cAddress;
    Wire.begin();
    _lcd = new LiquidCrystal_I2C(_address, 16, 2);
    _lcd->init();
    _lcd->backlight();
    _lcd->setCursor(0, 0);
    _lcd->print(F("AcouSense v1.0  "));
    _lcd->setCursor(0, 1);
    _lcd->print(F("Initializing... "));
    _lastRefreshTime = millis();
}

void LCDManager::update() {
    unsigned long now = millis();
    if (now - _lastRefreshTime < LCD_REFRESH_MS) return;
    _lastRefreshTime = now;

    uint16_t level = SensorManager::getCurrentLevel();
    uint8_t  alert = SensorManager::getCurrentAlertLevel();
    _render(level, alert);
}

void LCDManager::setConnectionState(ConnectionState state) {
    _connState = state;
}
