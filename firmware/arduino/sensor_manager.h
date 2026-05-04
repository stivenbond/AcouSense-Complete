/**
 * sensor_manager.h
 * AcouSense Arduino Firmware — Sensor Manager
 *
 * Continuously reads the microphone ADC, maintains rolling statistics,
 * and finalizes a report struct every 10 seconds.
 *
 * Spec: docs/specs/03_arduino_firmware_spec.md §3.1
 */

#pragma once
#include <stdint.h>
#include "packet_protocol.h"
#include "config_manager.h"

class SensorManager {
public:
    /** Initialize pin mode and internal state. Call once in setup(). */
    static void init();

    /**
     * Non-blocking update — call every loop() iteration.
     * Samples ADC at 20 Hz and finalizes report every 10 seconds.
     */
    static void update();

    /** Current instantaneous normalized level (0-100). */
    static uint16_t getCurrentLevel();

    /** Current alert level derived from config thresholds. */
    static uint8_t  getCurrentAlertLevel();

    /** True if a 10-second report is ready to transmit. */
    static bool isReportReady();

    /** Returns the pending report. Only valid when isReportReady() == true. */
    static AudioReportPayload getReport();

    /** Clear the ready flag after the report has been read/transmitted. */
    static void clearReport();

private:
    static uint16_t     _currentLevel;
    static uint16_t     _minLevel;
    static uint16_t     _maxLevel;
    static uint32_t     _sumLevel;
    static uint16_t     _sampleCount;
    static unsigned long _lastSampleTime;
    static unsigned long _lastReportTime;
    static bool          _reportReady;
    static AudioReportPayload _report;

    static uint16_t     _normalizeADC(int raw);
    static uint8_t      _computeAlertLevel(uint16_t level);
};
