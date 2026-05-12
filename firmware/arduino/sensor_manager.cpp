/**
 * sensor_manager.cpp
 * AcouSense Arduino Firmware — Sensor Manager
 */

/* Key Changes Explained
MIC_SLOPE_DBZ: Changed from 20.0f → 17.0041f (fitted A coefficient)
MIC_OFFSET_DBZ: Changed from 35.0f → 66.2440f (fitted B coefficient)
Added MIC_ADC_OFFSET: 8.5986f (fitted C coefficient - the noise floor)
MIC_NOISE_FLOOR_ADC: Changed from 2.0f → 0.5f (now works with offset subtraction)
MIC_MIN_DBZ: Changed from 30.0f → 52.0f (actual noise floor from data)
MIC_MAX_DBZ: Changed from 120.0f → 105.0f (more realistic, gives 6dB headroom above 99dB)
*/

#include "sensor_manager.h"
#include <Arduino.h>
#include <math.h>

#define MIC_PIN          A0
#define SAMPLE_INTERVAL  2UL      // ms — 500 Hz ADC envelope sampling
#define DISPLAY_INTERVAL 125UL    // ms — publish a stable LCD value
#define REPORT_INTERVAL  10000UL  // ms — 10 seconds
#define SERIAL_PRINT_INTERVAL 1000UL // ms

// Calibration knobs for the analog microphone module (dBZ).
// Fitted from actual measurements: dB = 17.0041 * log10(ADC_pp - 8.5986) + 66.2440
#define MIC_SLOPE_DBZ          17.0041f   // The 'A' in Y = A*log10(X-C) + B
#define MIC_OFFSET_DBZ         66.2440f   // The 'B' in Y = A*log10(X-C) + B
#define MIC_ADC_OFFSET         8.5986f    // The 'C' in Y = A*log10(X-C) + B
#define MIC_NOISE_FLOOR_ADC    0.5f       // Minimum corrected ADC value (avoid log(negative))
#define MIC_MIN_DBZ            52.0f      // Your measured noise floor
#define MIC_MAX_DBZ            105.0f     // Slightly above your max 99dB for headroom

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
uint16_t      SensorManager::_currentDbzTenths = (uint16_t)(MIC_MIN_DBZ * 10.0f);
int           SensorManager::_windowMinRaw   = 1023;
int           SensorManager::_windowMaxRaw   = 0;
unsigned long SensorManager::_lastWindowTime = 0;
unsigned long SensorManager::_lastSerialTime = 0;
int           SensorManager::_calMinRaw      = 1023;
int           SensorManager::_calMaxRaw      = 0;

// ─── Private helpers ─────────────────────────────────────────────────────────

uint16_t SensorManager::_adcPeakToDbzTenths(uint16_t peakToPeak) {
    // Subtract the ADC offset (sensor's noise floor)
    float signal = (float)peakToPeak - MIC_ADC_OFFSET;
    
    // Prevent log of negative or zero
    if (signal < MIC_NOISE_FLOOR_ADC) signal = MIC_NOISE_FLOOR_ADC;
    
    // dBZ Formula: dB = A * log10(ADC_corrected) + B
    // Where A = MIC_SLOPE_DBZ and B = MIC_OFFSET_DBZ
    float dbz = MIC_SLOPE_DBZ * log10f(signal) + MIC_OFFSET_DBZ;
    
    // Clamp to realistic range (your sensor's limits)
    if (dbz < MIC_MIN_DBZ) dbz = MIC_MIN_DBZ;
    if (dbz > MIC_MAX_DBZ) dbz = MIC_MAX_DBZ;
    
    return (uint16_t)(dbz * 10.0f + 0.5f);
}

uint8_t SensorManager::_computeAlertLevel(uint16_t dbz) {
    const Config& cfg = ConfigManager::get();
    if (dbz >= cfg.high_threshold)   return 3;
    if (dbz >= cfg.medium_threshold) return 2;
    if (dbz >= cfg.low_threshold)    return 1;
    return 0;
}

// ─── Public API ──────────────────────────────────────────────────────────────

void SensorManager::init() {
    pinMode(MIC_PIN, INPUT);
    _minLevel       = (uint16_t)MIC_MAX_DBZ;
    _maxLevel       = 0;
    _sumLevel       = 0;
    _sampleCount    = 0;
    _currentDbzTenths = (uint16_t)(MIC_MIN_DBZ * 10.0f);
    _currentLevel   = (uint16_t)MIC_MIN_DBZ;
    _windowMinRaw   = 1023;
    _windowMaxRaw   = 0;
    _calMinRaw      = 1023;
    _calMaxRaw      = 0;
    _reportReady    = false;
    _lastSampleTime = millis();
    _lastWindowTime = millis();
    _lastSerialTime = millis();
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
        if (raw < _calMinRaw)    _calMinRaw = raw;
        if (raw > _calMaxRaw)    _calMaxRaw = raw;
    }

    // ── Publish dBZ estimate for LCD/aggregation ────────────────────────
    if (now - _lastWindowTime >= DISPLAY_INTERVAL) {
        _lastWindowTime = now;

        uint16_t peakToPeak = (uint16_t)(_windowMaxRaw - _windowMinRaw);
        _currentDbzTenths = _adcPeakToDbzTenths(peakToPeak);
        _currentLevel = (uint16_t)((_currentDbzTenths + 5) / 10);

        if (_currentLevel < _minLevel) _minLevel = _currentLevel;
        if (_currentLevel > _maxLevel) _maxLevel = _currentLevel;
        _sumLevel += _currentLevel;
        _sampleCount++;

        _windowMinRaw = 1023;
        _windowMaxRaw = 0;
    }

    // ── Calibration output every second ──────────────────────────────────
    if (now - _lastSerialTime >= SERIAL_PRINT_INTERVAL) {
        _lastSerialTime = now;
        uint16_t calPeakToPeak = (uint16_t)(_calMaxRaw - _calMinRaw);
        Serial.print(F("CAL: ADC Raw Peak-to-Peak: "));
        Serial.println(calPeakToPeak);
        
        _calMinRaw = 1023;
        _calMaxRaw = 0;
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

uint16_t SensorManager::getCurrentDbzTenths() { return _currentDbzTenths; }

uint8_t SensorManager::getCurrentAlertLevel() {
    return _computeAlertLevel(_currentLevel);
}

bool SensorManager::isReportReady() { return _reportReady; }

AudioReportPayload SensorManager::getReport() { return _report; }

void SensorManager::clearReport() { _reportReady = false; }
