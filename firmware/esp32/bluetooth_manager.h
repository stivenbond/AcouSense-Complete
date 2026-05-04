/**
 * bluetooth_manager.h
 * AcouSense ESP32 Firmware — Bluetooth Manager
 *
 * Operates as BLE Central. Continuously scans for Android devices
 * advertising the AcouSense service UUID. Tracks presence of known
 * paired devices and delivers sync payloads when requested by SyncManager.
 *
 * Also handles the one-time pairing peripheral mode: temporarily switches
 * to Peripheral role so an Android device can write its user_identifier.
 *
 * Spec: docs/specs/04_esp32_firmware_spec.md §3.6
 *       docs/specs/07_bluetooth_sync_spec.md
 */

#pragma once
#include <stdint.h>
#include "ble_constants.h"

enum class BTManagerStatus : uint8_t {
    IDLE     = 0,
    SCANNING = 1,
    SYNCING  = 2,
    PAIRING  = 3,
};

struct PresenceEntry {
    char     mac[18];
    char     userId[64];
    bool     present;
    int64_t  ts_first_seen;  // Unix ts of first detection in current window
    int64_t  ts_last_seen;   // Unix ts of most recent detection
};

class BluetoothManager {
public:
    /** Initialize BLE stack and start scanning. Call once in setup(). */
    static void init();

    /**
     * Non-blocking update — call every loop() iteration.
     * Drives scan result processing and pairing state machine.
     */
    static void update();

    /** Return presence table (pointer + count) for SyncManager to iterate. */
    static PresenceEntry* getPresenceTable(int& countOut);

    /** Reset all presence flags — called by SyncManager after each sync cycle. */
    static void resetPresence();

    /**
     * Deliver a SyncPayload to a specific device by MAC address.
     * Connects, writes to 0xAC01, waits for ACK on 0xAC03, disconnects.
     * Returns true on successful delivery.
     */
    static bool deliverSync(const char* mac, const SyncPayload& payload);

    /** Current manager status string (for /api/status). */
    static const char* getStatusString();

    /** Unix timestamp of last completed sync cycle. */
    static int64_t getLastSyncTs();

    static void setLastSyncTs(int64_t ts);

private:
    static BTManagerStatus _status;
    static int64_t         _lastSyncTs;

    static PresenceEntry   _presence[32];  // Support up to 32 devices
    static int             _presenceCount;

    static void _onScanResult(const char* mac, const char* userId);
    static void _startScan();
};
