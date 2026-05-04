/**
 * database_manager.cpp
 * AcouSense ESP32 Firmware — Database Manager
 *
 * Uses the esp32-sqlite3 library (Siara-cc) which provides a VFS layer
 * that maps SQLite file operations to the SD card via SD.h.
 *
 * Library install: Arduino IDE → Library Manager → search "ESP32 SQLite3"
 * Repository: https://github.com/siara-cc/esp32-sqlite3
 */

#include "database_manager.h"
#include "sd_card_manager.h"
#include <sqlite3.h>
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

// ─── SQLite DB handle ────────────────────────────────────────────────────────

static sqlite3* _db = nullptr;
bool DatabaseManager::_ready = false;

// ─── Private helpers ─────────────────────────────────────────────────────────

bool DatabaseManager::_exec(const char* sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(_db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        Serial.print(F("[DB] SQL error: "));
        Serial.println(errMsg ? errMsg : "unknown");
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool DatabaseManager::_initSchema() {
    // WAL mode and sync settings for SD card reliability
    if (!_exec("PRAGMA journal_mode = WAL;"))   return false;
    if (!_exec("PRAGMA synchronous = NORMAL;")) return false;

    // noise_logs table
    if (!_exec(
        "CREATE TABLE IF NOT EXISTS noise_logs ("
        "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp   INTEGER NOT NULL,"
        "  min_level   INTEGER NOT NULL,"
        "  max_level   INTEGER NOT NULL,"
        "  avg_level   INTEGER NOT NULL,"
        "  alert_level INTEGER NOT NULL"
        ");")) return false;

    if (!_exec(
        "CREATE INDEX IF NOT EXISTS idx_noise_ts "
        "ON noise_logs(timestamp);")) return false;

    // configuration table
    if (!_exec(
        "CREATE TABLE IF NOT EXISTS configuration ("
        "  id                INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  low_threshold     INTEGER NOT NULL DEFAULT 40,"
        "  medium_threshold  INTEGER NOT NULL DEFAULT 60,"
        "  high_threshold    INTEGER NOT NULL DEFAULT 80,"
        "  pattern_low       INTEGER NOT NULL DEFAULT 1,"
        "  pattern_medium    INTEGER NOT NULL DEFAULT 2,"
        "  pattern_high      INTEGER NOT NULL DEFAULT 3"
        ");")) return false;

    // bluetooth_devices table
    if (!_exec(
        "CREATE TABLE IF NOT EXISTS bluetooth_devices ("
        "  id              INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  mac_address     TEXT NOT NULL UNIQUE,"
        "  user_identifier TEXT NOT NULL,"
        "  last_seen       INTEGER NOT NULL"
        ");")) return false;

    if (!_exec(
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_bt_mac "
        "ON bluetooth_devices(mac_address);")) return false;

    return true;
}

bool DatabaseManager::_ensureDefaultConfig() {
    // Insert defaults only if no configuration row exists
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(_db,
        "SELECT COUNT(*) FROM configuration;", -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (count == 0) {
        return _exec(
            "INSERT INTO configuration "
            "(low_threshold, medium_threshold, high_threshold, "
            " pattern_low, pattern_medium, pattern_high) "
            "VALUES (40, 60, 80, 1, 2, 3);");
    }
    return true;
}

// ─── Public API ──────────────────────────────────────────────────────────────

bool DatabaseManager::init() {
    if (!SDCardManager::isMounted()) {
        Serial.println(F("[DB] Cannot init — SD card not mounted"));
        return false;
    }

    // sqlite3_initialize is called by the library automatically
    int rc = sqlite3_open(SDCardManager::getDBPath(), &_db);
    if (rc != SQLITE_OK) {
        Serial.print(F("[DB] Failed to open DB: "));
        Serial.println(sqlite3_errmsg(_db));
        _db = nullptr;
        return false;
    }

    if (!_initSchema()) {
        Serial.println(F("[DB] Schema init failed"));
        return false;
    }

    if (!_ensureDefaultConfig()) {
        Serial.println(F("[DB] Default config insert failed"));
        return false;
    }

    _ready = true;
    Serial.println(F("[DB] Database ready"));
    return true;
}

bool DatabaseManager::isReady() { return _ready; }

// ── noise_logs ────────────────────────────────────────────────────────────────

bool DatabaseManager::insertReading(const AudioReportPayload& report,
                                    int64_t unixTimestamp) {
    if (!_ready) return false;

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(_db,
        "INSERT INTO noise_logs (timestamp, min_level, max_level, avg_level, alert_level) "
        "VALUES (?, ?, ?, ?, ?);", -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_int64(stmt, 1, unixTimestamp);
    sqlite3_bind_int(stmt,   2, report.min_level);
    sqlite3_bind_int(stmt,   3, report.max_level);
    sqlite3_bind_int(stmt,   4, report.avg_level);
    sqlite3_bind_int(stmt,   5, report.alert_level);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

int DatabaseManager::getReadings(int64_t fromTs, int64_t toTs,
                                  int limit, int offset,
                                  NoiseRecordCallback cb, void* userdata) {
    if (!_ready || !cb) return 0;

    char sql[256];
    if (fromTs > 0 && toTs > 0) {
        snprintf(sql, sizeof(sql),
            "SELECT id,timestamp,min_level,max_level,avg_level,alert_level "
            "FROM noise_logs WHERE timestamp >= %lld AND timestamp <= %lld "
            "ORDER BY timestamp ASC LIMIT %d OFFSET %d;",
            fromTs, toTs, limit > 0 ? limit : 10000, offset);
    } else {
        snprintf(sql, sizeof(sql),
            "SELECT id,timestamp,min_level,max_level,avg_level,alert_level "
            "FROM noise_logs ORDER BY timestamp ASC LIMIT %d OFFSET %d;",
            limit > 0 ? limit : 10000, offset);
    }

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        NoiseRecord rec;
        rec.id          = sqlite3_column_int64(stmt, 0);
        rec.timestamp   = sqlite3_column_int64(stmt, 1);
        rec.min_level   = sqlite3_column_int(stmt,   2);
        rec.max_level   = sqlite3_column_int(stmt,   3);
        rec.avg_level   = sqlite3_column_int(stmt,   4);
        rec.alert_level = sqlite3_column_int(stmt,   5);
        cb(rec, userdata);
        count++;
    }
    sqlite3_finalize(stmt);
    return count;
}

int DatabaseManager::countReadings(int64_t fromTs, int64_t toTs) {
    if (!_ready) return 0;
    char sql[200];
    if (fromTs > 0 && toTs > 0) {
        snprintf(sql, sizeof(sql),
            "SELECT COUNT(*) FROM noise_logs "
            "WHERE timestamp >= %lld AND timestamp <= %lld;", fromTs, toTs);
    } else {
        snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM noise_logs;");
    }
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;
    int count = (sqlite3_step(stmt) == SQLITE_ROW) ? sqlite3_column_int(stmt, 0) : 0;
    sqlite3_finalize(stmt);
    return count;
}

// ── configuration ─────────────────────────────────────────────────────────────

bool DatabaseManager::getConfig(DBConfig& out) {
    if (!_ready) return false;
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(_db,
        "SELECT low_threshold, medium_threshold, high_threshold, "
        "pattern_low, pattern_medium, pattern_high "
        "FROM configuration WHERE id=1;", -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out.low_threshold    = sqlite3_column_int(stmt, 0);
        out.medium_threshold = sqlite3_column_int(stmt, 1);
        out.high_threshold   = sqlite3_column_int(stmt, 2);
        out.pattern_low      = sqlite3_column_int(stmt, 3);
        out.pattern_medium   = sqlite3_column_int(stmt, 4);
        out.pattern_high     = sqlite3_column_int(stmt, 5);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

bool DatabaseManager::saveConfig(const DBConfig& cfg) {
    if (!_ready) return false;
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(_db,
        "UPDATE configuration SET "
        "low_threshold=?, medium_threshold=?, high_threshold=?, "
        "pattern_low=?, pattern_medium=?, pattern_high=? WHERE id=1;",
        -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, cfg.low_threshold);
    sqlite3_bind_int(stmt, 2, cfg.medium_threshold);
    sqlite3_bind_int(stmt, 3, cfg.high_threshold);
    sqlite3_bind_int(stmt, 4, cfg.pattern_low);
    sqlite3_bind_int(stmt, 5, cfg.pattern_medium);
    sqlite3_bind_int(stmt, 6, cfg.pattern_high);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

// ── bluetooth_devices ─────────────────────────────────────────────────────────

bool DatabaseManager::upsertDevice(const char* mac, const char* userId, int64_t lastSeen) {
    if (!_ready) return false;
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(_db,
        "INSERT INTO bluetooth_devices (mac_address, user_identifier, last_seen) "
        "VALUES (?, ?, ?) "
        "ON CONFLICT(mac_address) DO UPDATE SET "
        "user_identifier=excluded.user_identifier, last_seen=excluded.last_seen;",
        -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt,  1, mac,    -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt,  2, userId, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, lastSeen);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

int DatabaseManager::getDevices(BTDeviceCallback cb, void* userdata) {
    if (!_ready || !cb) return 0;
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(_db,
        "SELECT id, mac_address, user_identifier, last_seen "
        "FROM bluetooth_devices ORDER BY last_seen DESC;",
        -1, &stmt, nullptr) != SQLITE_OK) return 0;

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        BTDevice dev;
        dev.id = sqlite3_column_int64(stmt, 0);
        strncpy(dev.mac_address,    (const char*)sqlite3_column_text(stmt, 1), 17);
        strncpy(dev.user_identifier,(const char*)sqlite3_column_text(stmt, 2), 63);
        dev.last_seen = sqlite3_column_int64(stmt, 3);
        dev.mac_address[17] = 0;
        dev.user_identifier[63] = 0;
        cb(dev, userdata);
        count++;
    }
    sqlite3_finalize(stmt);
    return count;
}

bool DatabaseManager::findDevice(const char* mac, BTDevice& out) {
    if (!_ready) return false;
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(_db,
        "SELECT id, mac_address, user_identifier, last_seen "
        "FROM bluetooth_devices WHERE mac_address = ?;",
        -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, mac, -1, SQLITE_STATIC);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out.id = sqlite3_column_int64(stmt, 0);
        strncpy(out.mac_address,    (const char*)sqlite3_column_text(stmt, 1), 17);
        strncpy(out.user_identifier,(const char*)sqlite3_column_text(stmt, 2), 63);
        out.last_seen = sqlite3_column_int64(stmt, 3);
        out.mac_address[17] = 0;
        out.user_identifier[63] = 0;
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}
