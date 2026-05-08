/**
 * sensor_manager.cpp
 * AcouSense Arduino Firmware — Sensor Manager
 */

#include "sensor_manager.h"
#include <Arduino.h>
#include <math.h>

#define MIC_PIN          A0
#define SAMPLE_INTERVAL  2UL      // ms — 500 Hz ADC envelope sampling
#define DISPLAY_INTERVAL 125UL    // ms — publish a stable LCD value
#define REPORT_INTERVAL  10000UL  // ms — 10 seconds

// Calibration knobs for the analog microphone module.
// Adjust MIC_CAL_P2P_ADC after comparing against a real SPL meter.
#define MIC_NOISE_FLOOR_ADC  2.0f
#define MIC_CAL_P2P_ADC     24.0f
#define MIC_CAL_DBA         60.0f
#define MIC_MIN_DBA         30.0f
#define MIC_MAX_DBA        120.0f

// ─── Static member definitions ───────────────────────────────────────────────

uint16_t      SensorManager::_currentLevel   = 0;
uint16_t      SensorManager::_minLevel       = 120;
uint16_t      SensorManager::_maxLevel       = 0;
uint32_t      SensorManager::_sumLevel       = 0;
uint16_t      SensorManager::_sampleCount    = 0;
unsigned long SensorManager::_lastSampleTime = 0;
unsigned long SensorManager::_lastReportTime = 0;
bool          SensorManager::_reportReady    = false;
AudioReportPayload SensorManager::_report    = {};
uint16_t      SensorManager::_currentDbaTenths = (uint16_t)(MIC_MIN_DBA * 10.0f);
int           SensorManager::_windowMinRaw   = 1023;
int           SensorManager::_windowMaxRaw   = 0;
unsigned long SensorManager::_lastWindowTime = 0;

// ─── Private helpers ─────────────────────────────────────────────────────────

uint16_t SensorManager::_adcPeakToDbaTenths(uint16_t peakToPeak) {
    float signal = (float)peakToPeak;
    if (signal < MIC_NOISE_FLOOR_ADC) signal = MIC_NOISE_FLOOR_ADC;

    float dba = MIC_CAL_DBA + 20.0f * log10f(signal / MIC_CAL_P2P_ADC);
    if (dba < MIC_MIN_DBA) dba = MIC_MIN_DBA;
    if (dba > MIC_MAX_DBA) dba = MIC_MAX_DBA;
    return (uint16_t)(dba * 10.0f + 0.5f);
}

uint8_t SensorManager::_computeAlertLevel(uint16_t dba) {
    const Config& cfg = ConfigManager::get();
    if (dba >= cfg.high_threshold)   return 3;
    if (dba >= cfg.medium_threshold) return 2;
    if (dba >= cfg.low_threshold)    return 1;
    return 0;
}

// ─── Public API ──────────────────────────────────────────────────────────────

void SensorManager::init() {
    pinMode(MIC_PIN, INPUT);
    _minLevel       = (uint16_t)MIC_MAX_DBA;
    _maxLevel       = 0;
    _sumLevel       = 0;
    _sampleCount    = 0;
    _currentDbaTenths = (uint16_t)(MIC_MIN_DBA * 10.0f);
    _currentLevel   = (uint16_t)MIC_MIN_DBA;
    _windowMinRaw   = 1023;
    _windowMaxRaw   = 0;
    _reportReady    = false;
    _lastSampleTime = millis();
    _lastWindowTime = millis();
    _lastReportTime = millis();
}

void SensorManager::update() {
    unsigned long now = millis();

    // ── Sample ADC envelope at 500 Hz ────────────────────────────────────
    if (now - _lastSampleTime >= SAMPLE_INTERVAL) {
        _lastSampleTime = now;

        int raw = analogRead(MIC_PIN);
        if (raw < _windowMinRaw) _windowMinRaw = raw;
        if (raw > _windowMaxRaw) _windowMaxRaw = raw;
    }

    // ── Publish dBA estimate for LCD/aggregation ────────────────────────
    if (now - _lastWindowTime >= DISPLAY_INTERVAL) {
        _lastWindowTime = now;

        uint16_t peakToPeak = (uint16_t)(_windowMaxRaw - _windowMinRaw);
        _currentDbaTenths = _adcPeakToDbaTenths(peakToPeak);
        _currentLevel = (uint16_t)((_currentDbaTenths + 5) / 10);

        if (_currentLevel < _minLevel) _minLevel = _currentLevel;
        if (_currentLevel > _maxLevel) _maxLevel = _currentLevel;
        _sumLevel += _currentLevel;
        _sampleCount++;

        _windowMinRaw = 1023;
        _windowMaxRaw = 0;
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

uint16_t SensorManager::getCurrentDbaTenths() { return _currentDbaTenths; }

uint8_t SensorManager::getCurrentAlertLevel() {
    return _computeAlertLevel(_currentLevel);
}

bool SensorManager::isReportReady() { return _reportReady; }

AudioReportPayload SensorManager::getReport() { return _report; }

void SensorManager::clearReport() { _reportReady = false; }
