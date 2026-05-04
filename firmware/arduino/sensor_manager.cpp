/**
 * sensor_manager.cpp
 * AcouSense Arduino Firmware — Sensor Manager
 */

#include "sensor_manager.h"
#include <Arduino.h>

#define MIC_PIN          A0
#define SAMPLE_INTERVAL  50UL     // ms — 20 Hz
#define REPORT_INTERVAL  10000UL  // ms — 10 seconds

// ─── Static member definitions ───────────────────────────────────────────────

uint16_t      SensorManager::_currentLevel   = 0;
uint16_t      SensorManager::_minLevel       = 100;
uint16_t      SensorManager::_maxLevel       = 0;
uint32_t      SensorManager::_sumLevel       = 0;
uint16_t      SensorManager::_sampleCount    = 0;
unsigned long SensorManager::_lastSampleTime = 0;
unsigned long SensorManager::_lastReportTime = 0;
bool          SensorManager::_reportReady    = false;
AudioReportPayload SensorManager::_report    = {};

// ─── Private helpers ─────────────────────────────────────────────────────────

uint16_t SensorManager::_normalizeADC(int raw) {
    // Map 0-1023 to 0-100
    if (raw <= 0)    return 0;
    if (raw >= 1023) return 100;
    return (uint16_t)((long)raw * 100L / 1023L);
}

uint8_t SensorManager::_computeAlertLevel(uint16_t level) {
    const Config& cfg = ConfigManager::get();
    if (level >= cfg.high_threshold)   return 3;
    if (level >= cfg.medium_threshold) return 2;
    if (level >= cfg.low_threshold)    return 1;
    return 0;
}

// ─── Public API ──────────────────────────────────────────────────────────────

void SensorManager::init() {
    pinMode(MIC_PIN, INPUT);
    _minLevel       = 100;
    _maxLevel       = 0;
    _sumLevel       = 0;
    _sampleCount    = 0;
    _currentLevel   = 0;
    _reportReady    = false;
    _lastSampleTime = millis();
    _lastReportTime = millis();
}

void SensorManager::update() {
    unsigned long now = millis();

    // ── Sample ADC at 20 Hz ──────────────────────────────────────────────
    if (now - _lastSampleTime >= SAMPLE_INTERVAL) {
        _lastSampleTime = now;

        int raw = analogRead(MIC_PIN);
        uint16_t level = _normalizeADC(raw);
        _currentLevel = level;

        if (level < _minLevel) _minLevel = level;
        if (level > _maxLevel) _maxLevel = level;
        _sumLevel += level;
        _sampleCount++;
    }

    // ── Finalize report every 10 seconds ────────────────────────────────
    if (now - _lastReportTime >= REPORT_INTERVAL) {
        _lastReportTime = now;

        if (_sampleCount > 0) {
            uint16_t avg = (uint16_t)(_sumLevel / _sampleCount);

            _report.timestamp   = (uint32_t)(now / 1000UL);
            _report.min_level   = _minLevel;
            _report.max_level   = _maxLevel;
            _report.avg_level   = avg;
            _report.alert_level = _computeAlertLevel(avg);
            _reportReady = true;
        }

        // Reset window
        _minLevel    = 100;
        _maxLevel    = 0;
        _sumLevel    = 0;
        _sampleCount = 0;
    }
}

uint16_t SensorManager::getCurrentLevel() { return _currentLevel; }

uint8_t SensorManager::getCurrentAlertLevel() {
    return _computeAlertLevel(_currentLevel);
}

bool SensorManager::isReportReady() { return _reportReady; }

AudioReportPayload SensorManager::getReport() { return _report; }

void SensorManager::clearReport() { _reportReady = false; }
