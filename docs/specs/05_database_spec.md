# SQLite Database Specification

## 1. Overview

The ESP32 stores all AcouSense historical data in a single SQLite database file located on the SD card. SQLite provides a zero-infrastructure persistence layer that survives power cycles and supports the query patterns required by both the web dashboard and the Bluetooth sync system.

**Database file path on SD card**: `/acousense/acousense.db`  
**SQLite library**: `ESP32-sqlite3` (arduino-esp32-sqlite3)

---

## 2. Design Principles

- Schema must remain minimal and stable. Adding columns later requires migration scripts.
- Every table uses `INTEGER PRIMARY KEY` for auto-increment.
- Write integrity is enforced via `BEGIN TRANSACTION / COMMIT`.
- `PRAGMA journal_mode = WAL;` is set on open to reduce corruption risk on unexpected power loss.
- `PRAGMA synchronous = NORMAL;` balances write speed with safety on SD media.
- No foreign key relationships are enforced at DB level (ESP32 SQLite builds often exclude FK support) — integrity is managed at the application layer.

---

## 3. Schema

### 3.1 `noise_logs`

Stores every 10-second aggregated reading received from the Arduino.

```sql
CREATE TABLE IF NOT EXISTS noise_logs (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp   INTEGER NOT NULL,   -- Unix timestamp (seconds)
    min_level   INTEGER NOT NULL,   -- Normalized 0-100 scale
    max_level   INTEGER NOT NULL,
    avg_level   INTEGER NOT NULL,
    alert_level INTEGER NOT NULL    -- 0=none, 1=low, 2=medium, 3=high
);

CREATE INDEX IF NOT EXISTS idx_noise_logs_timestamp ON noise_logs(timestamp);
```

**Notes**:
- `timestamp` is epoch seconds, set by the ESP32's system clock (synced via NTP if available, otherwise millis-based offset)
- The index on `timestamp` is critical for efficient range queries used by the web dashboard

---

### 3.2 `configuration`

Stores the single active configuration row. Only one row ever exists (id=1).

```sql
CREATE TABLE IF NOT EXISTS configuration (
    id                INTEGER PRIMARY KEY AUTOINCREMENT,
    low_threshold     INTEGER NOT NULL DEFAULT 400,
    medium_threshold  INTEGER NOT NULL DEFAULT 600,
    high_threshold    INTEGER NOT NULL DEFAULT 800,
    pattern_low       INTEGER NOT NULL DEFAULT 1,
    pattern_medium    INTEGER NOT NULL DEFAULT 2,
    pattern_high      INTEGER NOT NULL DEFAULT 3
);
```

**Access pattern**:
- On boot: `SELECT * FROM configuration WHERE id=1`
- On update: `UPDATE configuration SET ... WHERE id=1`
- On first run (no rows): `INSERT INTO configuration DEFAULT VALUES`

---

### 3.3 `bluetooth_devices`

Stores all known paired Android devices and tracks presence.

```sql
CREATE TABLE IF NOT EXISTS bluetooth_devices (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    mac_address     TEXT NOT NULL UNIQUE,
    user_identifier TEXT NOT NULL,
    last_seen       INTEGER NOT NULL    -- Unix timestamp of last BLE detection
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_bt_mac ON bluetooth_devices(mac_address);
```

**Access pattern**:
- Lookup: `SELECT * FROM bluetooth_devices WHERE mac_address = ?`
- Upsert: `INSERT OR REPLACE INTO bluetooth_devices (mac_address, user_identifier, last_seen) VALUES (?, ?, ?)`
- Dashboard listing: `SELECT * FROM bluetooth_devices ORDER BY last_seen DESC`

---

## 4. Query Reference

### 4.1 Web Dashboard Queries

**Live status** (last reading):
```sql
SELECT * FROM noise_logs ORDER BY timestamp DESC LIMIT 1;
```

**Daily history**:
```sql
SELECT * FROM noise_logs WHERE timestamp >= ? AND timestamp < ? ORDER BY timestamp ASC;
```

**Weekly aggregated view**:
```sql
SELECT
    (timestamp / 3600) * 3600 AS hour_bucket,
    MIN(min_level),
    MAX(max_level),
    AVG(avg_level)
FROM noise_logs
WHERE timestamp >= ? AND timestamp < ?
GROUP BY hour_bucket
ORDER BY hour_bucket ASC;
```

**CSV export** (all records):
```sql
SELECT id, timestamp, min_level, max_level, avg_level, alert_level FROM noise_logs ORDER BY timestamp ASC;
```

---

### 4.2 Sync Manager Queries

**5-minute window for sync aggregation**:
```sql
SELECT MIN(min_level), MAX(max_level), AVG(avg_level)
FROM noise_logs
WHERE timestamp >= ? AND timestamp < ?;
```

---

## 5. Initialization Sequence

On every ESP32 boot, the DatabaseManager must:

1. Open (or create) `/acousense/acousense.db`
2. Execute: `PRAGMA journal_mode = WAL;`
3. Execute: `PRAGMA synchronous = NORMAL;`
4. Run `CREATE TABLE IF NOT EXISTS` for all three tables
5. Run `CREATE INDEX IF NOT EXISTS` for all indexes
6. Check if `configuration` table has a row; if not, insert defaults
7. Load configuration into ConfigManager memory

---

## 6. Data Retention

- No automatic data expiry is specified in v1
- The web dashboard provides CSV export for archiving
- SD card capacity determines practical retention limit
  - A 10-second writes means ~8,640 rows/day → ~3M rows/year
  - Each row ≈ 40 bytes → ~120 MB/year — fits comfortably on an 8GB SD card
- A manual purge endpoint (`DELETE FROM noise_logs WHERE timestamp < ?`) may be exposed via the web API in a future version

---

## 7. Validation Checklist

- [ ] DB file is created on first boot at correct path
- [ ] WAL mode is confirmed active
- [ ] All three tables exist after initialization
- [ ] `configuration` row exists with defaults after first boot
- [ ] `insertReading()` adds rows correctly and they are queryable immediately
- [ ] Range queries return correct rows for known timestamp windows
- [ ] `upsertDevice()` updates `last_seen` without creating duplicates
- [ ] DB remains consistent after simulated power-off during a write
