/**
 * database_manager.h
 * AcouSense ESP32 Firmware — Database Manager
 *
 * Manages SQLite database on SD card. Provides typed insert and query
 * operations for all three tables: noise_logs, configuration, bluetooth_devices.
 *
 * Library: esp32-sqlite3 by Siara-cc (install via Arduino Library Manager)
 * SQLite path: /acousense/acousense.db (via SDCardManager)
 *
 * Spec: docs/specs/04_esp32_firmware_spec.md §3.3
 *       docs/specs/05_database_spec.md
 */

#pragma once
#include <stdint.h>
#include "packet_protocol.h"

// ─── Data Transfer Objects ───────────────────────────────────────────────────

struct NoiseRecord {
    int64_t  id;
    int64_t  timestamp;
    int32_t  min_level;
    int32_t  max_level;
    int32_t  avg_level;
    int32_t  alert_level;
};

struct DBConfig {
    int32_t  low_threshold;
    int32_t  medium_threshold;
    int32_t  high_threshold;
    int32_t  pattern_low;
    int32_t  pattern_medium;
    int32_t  pattern_high;
};

struct BTDevice {
    int64_t  id;
    char     mac_address[18];    // "AA:BB:CC:DD:EE:FF\0"
    char     user_identifier[64];
    int64_t  last_seen;
};

// ─── Query result callback type ──────────────────────────────────────────────
// Called once per result row during getReadings()
typedef void (*NoiseRecordCallback)(const NoiseRecord& rec, void* userdata);
typedef void (*BTDeviceCallback)(const BTDevice& dev, void* userdata);

class DatabaseManager {
public:
    /**
     * Open (or create) the database and initialize schema.
     * Must be called after SDCardManager::init() succeeds.
     * Returns true on success.
     */
    static bool init();

    /** True if the database is open and usable. */
    static bool isReady();

    // ── noise_logs ────────────────────────────────────────────────────────

    /** Insert one 10-second audio reading. Returns true on success. */
    static bool insertReading(const AudioReportPayload& report, int64_t unixTimestamp);

    /**
     * Iterate readings in [fromTs, toTs] inclusive.
     * Calls cb(record, userdata) for each row.
     * Pass limit=0 for no limit.
     */
    static int  getReadings(int64_t fromTs, int64_t toTs,
                            int limit, int offset,
                            NoiseRecordCallback cb, void* userdata);

    /** Count total rows in noise_logs (optionally filtered by range). */
    static int  countReadings(int64_t fromTs, int64_t toTs);

    // ── configuration ─────────────────────────────────────────────────────

    /** Load config from DB into out. Returns false if no row exists. */
    static bool getConfig(DBConfig& out);

    /** Save (upsert) config. Returns true on success. */
    static bool saveConfig(const DBConfig& cfg);

    // ── bluetooth_devices ─────────────────────────────────────────────────

    /**
     * Upsert a Bluetooth device record.
     * If MAC already exists, updates user_identifier and last_seen.
     */
    static bool upsertDevice(const char* mac, const char* userId, int64_t lastSeen);

    /** Iterate all known BT devices ordered by last_seen DESC. */
    static int  getDevices(BTDeviceCallback cb, void* userdata);

    /** Find one device by MAC. Returns false if not found. */
    static bool findDevice(const char* mac, BTDevice& out);

private:
    static bool _ready;
    static bool _initSchema();
    static bool _exec(const char* sql);
    static bool _ensureDefaultConfig();
};
