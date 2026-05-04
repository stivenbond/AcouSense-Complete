/**
 * sync_manager.h
 * AcouSense ESP32 Firmware — Sync Manager
 *
 * Every 5 minutes, aggregates noise_logs data for the window, builds
 * a personalized SyncPayload for each present device, and hands it to
 * BluetoothManager for delivery.
 *
 * Spec: docs/specs/04_esp32_firmware_spec.md §3.7
 *       docs/specs/07_bluetooth_sync_spec.md §4–7
 */

#pragma once
#include <stdint.h>

class SyncManager {
public:
    /** Reset the sync cycle timer. Call once in setup(). */
    static void init();

    /**
     * Non-blocking update — call every loop() iteration.
     * Triggers sync cycle every 5 minutes.
     */
    static void update();

private:
    static unsigned long _lastCycleTime;
    static void _runCycle();
};
