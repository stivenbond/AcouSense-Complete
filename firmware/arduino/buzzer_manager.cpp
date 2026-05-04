/**
 * buzzer_manager.cpp
 * AcouSense Arduino Firmware — Buzzer Manager
 */

#include "buzzer_manager.h"
#include "sensor_manager.h"
#include "config_manager.h"
#include <Arduino.h>

#define BUZZER_PIN   9
#define TONE_FREQ    2000  // Hz — standard warning tone

// ─── Static member definitions ───────────────────────────────────────────────

unsigned long BuzzerManager::_cycleStart  = 0;
uint8_t       BuzzerManager::_lastPattern = 0xFF; // Force init on first update

// ─── Pattern implementations ─────────────────────────────────────────────────

void BuzzerManager::_pattern00(unsigned long /*elapsed*/) {
    noTone(BUZZER_PIN);
}

// ON 100ms → OFF 2900ms (period = 3000ms)
void BuzzerManager::_pattern01(unsigned long elapsed) {
    uint32_t t = elapsed % 3000UL;
    if (t < 100) tone(BUZZER_PIN, TONE_FREQ);
    else         noTone(BUZZER_PIN);
}

// ON100ms → OFF200ms → ON100ms → OFF1600ms (period = 2000ms)
void BuzzerManager::_pattern02(unsigned long elapsed) {
    uint32_t t = elapsed % 2000UL;
    if      (t < 100)  tone(BUZZER_PIN, TONE_FREQ);
    else if (t < 300)  noTone(BUZZER_PIN);
    else if (t < 400)  tone(BUZZER_PIN, TONE_FREQ);
    else               noTone(BUZZER_PIN);
}

// ON80ms → OFF80ms continuously (period = 160ms)
void BuzzerManager::_pattern03(unsigned long elapsed) {
    uint32_t t = elapsed % 160UL;
    if (t < 80) tone(BUZZER_PIN, TONE_FREQ);
    else        noTone(BUZZER_PIN);
}

// ─── Private dispatch ────────────────────────────────────────────────────────

void BuzzerManager::_applyPattern(uint8_t pattern, unsigned long now) {
    // Reset cycle start when pattern changes
    if (pattern != _lastPattern) {
        noTone(BUZZER_PIN);
        _cycleStart  = now;
        _lastPattern = pattern;
    }

    unsigned long elapsed = now - _cycleStart;

    switch (pattern) {
        case 0x00: _pattern00(elapsed); break;
        case 0x01: _pattern01(elapsed); break;
        case 0x02: _pattern02(elapsed); break;
        case 0x03: _pattern03(elapsed); break;
        default:   noTone(BUZZER_PIN);  break;
    }
}

// ─── Public API ──────────────────────────────────────────────────────────────

void BuzzerManager::init() {
    pinMode(BUZZER_PIN, OUTPUT);
    noTone(BUZZER_PIN);
    _cycleStart  = millis();
    _lastPattern = 0xFF;
}

void BuzzerManager::update() {
    unsigned long now = millis();
    uint8_t alertLevel = SensorManager::getCurrentAlertLevel();

    const Config& cfg = ConfigManager::get();
    uint8_t pattern;

    switch (alertLevel) {
        case 1:  pattern = cfg.pattern_low;    break;
        case 2:  pattern = cfg.pattern_medium; break;
        case 3:  pattern = cfg.pattern_high;   break;
        default: pattern = 0x00;               break;
    }

    _applyPattern(pattern, now);
}
