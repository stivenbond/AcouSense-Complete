/**
 * buzzer_manager.h
 * AcouSense Arduino Firmware — Buzzer Manager
 *
 * Non-blocking PWM buzzer controller. Evaluates the current alert level
 * from SensorManager and applies the configured pattern from ConfigManager.
 * All timing uses millis() — no delay() calls.
 *
 * Spec: docs/specs/03_arduino_firmware_spec.md §3.3
 */

#pragma once
#include <stdint.h>

class BuzzerManager {
public:
    /** Initialize buzzer pin. Call once in setup(). */
    static void init();

    /**
     * Non-blocking update — call every loop() iteration.
     * Reads current alert level and drives the buzzer pattern.
     */
    static void update();

private:
    // Pattern state machine
    static unsigned long _cycleStart;  // Start of current pattern cycle
    static uint8_t       _lastPattern; // Last applied pattern — detects changes

    static void _applyPattern(uint8_t pattern, unsigned long now);

    // Pattern implementations — all operate on elapsed time within cycle
    static void _pattern00(unsigned long elapsed);  // Silent
    static void _pattern01(unsigned long elapsed);  // Single beep/3s
    static void _pattern02(unsigned long elapsed);  // Double pulse/2s
    static void _pattern03(unsigned long elapsed);  // Rapid alarm
};
