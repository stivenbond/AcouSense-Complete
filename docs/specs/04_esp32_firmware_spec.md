# ESP32 Firmware Specification

## 1. Overview

The ESP32 firmware is the system coordinator of the AcouSense ecosystem. It bridges the hardware sensor layer (via SPI) with the storage layer (SQLite on SD), the administration layer (web dashboard over Wi-Fi), and the user layer (Bluetooth/BLE synchronization with Android devices).

The firmware architecture separates transport logic from application logic. Module failures must be isolated — a Bluetooth failure must not affect SD writes; a Wi-Fi drop must not interrupt SPI polling.

**Framework**: Arduino framework for ESP32 (via ESP32 Arduino Core) is acceptable for development speed. ESP-IDF provides superior long-term stability if the project scope expands.

---

## 2. Architecture

```
main loop()
├── SPIMasterManager::update()      — poll Arduino, handle reports
├── DatabaseManager                 — called on report receipt
├── WebServer                       — handles incoming HTTP requests
├── BluetoothManager::update()      — BLE scan, connect, sync cycle
└── SyncManager::update()           — 5-minute aggregation + push

Background / Init:
├── SDCardManager::init()
├── ConfigManager::load()           — restore config from SQLite on boot
└── WiFi connection management
```

All modules use `millis()` scheduling. No blocking calls are permitted in the main loop.

---

## 3. Module Specifications

### 3.1 SPIMasterManager

**Responsibility**: Poll the Arduino for audio reports, push configuration changes, and manage heartbeat verification.

**Behavior**:
- Every **10 seconds**: assert CS, transmit a 0x04 Heartbeat, read the Arduino response buffer for a pending 0x01 Audio Report
- When the web dashboard submits a new configuration: immediately send a 0x02 Configuration Packet and wait for ACK (up to 3 retries within 500ms each)
- If 3 consecutive heartbeats receive no ACK: set `arduinoStatus = UNRESPONSIVE`; log to SD; web dashboard reflects OFFLINE state
- Build and parse packets using shared `packet_protocol` utilities

**SPI initialization**:
```cpp
SPI.begin(18, 19, 23, 5);  // SCLK, MISO, MOSI, CS
SPI.beginTransaction(SPISettings(500000, MSBFIRST, SPI_MODE0));
```

**Files**: `spi_master_manager.h`, `spi_master_manager.cpp`

---

### 3.2 SDCardManager

**Responsibility**: Initialize and maintain access to the SD card, which hosts the SQLite database file.

**Behavior**:
- Mount SD on boot using `SD.begin(CS_PIN)`
- If mount fails: log error, set `sdStatus = FAILED`, continue (system degrades gracefully — web shows SD ERROR)
- Expose `getRootPath()` for the DatabaseManager to construct file paths
- Provide `isMounted()` health check called by telemetry/status endpoints

**Files**: `sd_card_manager.h`, `sd_card_manager.cpp`

---

### 3.3 DatabaseManager

**Responsibility**: Store all received audio reports in SQLite on the SD card, manage schema initialization, and expose query functions for web and sync consumers.

**SQLite Library**: `ESP32-sqlite3` (arduino-esp32-sqlite3 or ArduinoSQLite)

**Schema** (see full spec in `05_database_spec.md`):
```sql
noise_logs (id, timestamp, min_level, max_level, avg_level, alert_level)
configuration (id, low_threshold, medium_threshold, high_threshold, pattern_low, pattern_medium, pattern_high)
bluetooth_devices (id, mac_address, user_identifier, last_seen)
```

**Behavior**:
- On boot: open or create `acousense.db` on SD card; run `CREATE TABLE IF NOT EXISTS` for all three tables
- `insertReading(AudioReport)`: insert a row into `noise_logs`
- `getReadings(from_ts, to_ts)`: return rows in timestamp range
- `getConfig()` / `saveConfig(Config)`: read/write `configuration` table
- `getDevices()`: return all rows from `bluetooth_devices`
- `upsertDevice(mac, userId, lastSeen)`: insert or update on MAC match
- All writes wrapped in `BEGIN; ... COMMIT;` transactions
- On failure: log error; do not crash — DatabaseManager reports write failure upward

**Files**: `database_manager.h`, `database_manager.cpp`

---

### 3.4 ConfigManager

**Responsibility**: Provide the active system configuration to all ESP32 modules. Persist changes to the `configuration` SQLite table.

**Behavior**:
- On boot: load config from `configuration` table via DatabaseManager
- If no row exists: insert defaults and use them
- Expose `getConfig()` returning a `Config` struct
- `applyConfig(Config)`: save to DB and trigger SPIMasterManager to push 0x02 packet to Arduino
- Configuration changes survive power cycles because they are written to SQLite

**Default values**:
```c
low_threshold    = 400
medium_threshold = 600
high_threshold   = 800
pattern_low      = 0x01
pattern_medium   = 0x02
pattern_high     = 0x03
```

**Files**: `config_manager.h`, `config_manager.cpp`

---

### 3.5 WebServer

**Responsibility**: Serve the SvelteKit web dashboard as static files and respond to REST API requests.

**Implementation**: Use `ESPAsyncWebServer` library.

**Static Files**: Dashboard build output served from `/www/` directory on SD card.

**REST API Endpoints**:

| Method | Path | Description |
|---|---|---|
| GET | `/api/status` | System health: Wi-Fi, SD, Arduino, BT, last reading |
| GET | `/api/readings` | Historical readings; supports `?from=&to=&limit=` query params |
| GET | `/api/readings/export` | CSV download of all or filtered readings |
| GET | `/api/config` | Current thresholds and buzzer patterns |
| POST | `/api/config` | Update config; triggers SPI push to Arduino |
| GET | `/api/devices` | All known paired BLE devices |

All responses use `Content-Type: application/json`.  
CSV export uses `Content-Type: text/csv` with `Content-Disposition: attachment; filename=export.csv`.

**Files**: `web_server.h`, `web_server.cpp`

---

### 3.6 BluetoothManager

**Responsibility**: Perform periodic BLE scanning, identify known AcouSense app devices, and manage temporary connections for data synchronization.

**BLE Architecture**: The ESP32 operates as a BLE Central (scanner/client) during discovery. Each paired Android device runs a BLE GATT server advertising a known service UUID.

**AcouSense BLE Service UUID**: `0xAC00` (custom 16-bit, expanded to full 128-bit UUID)

**Behavior**:
- Scan continuously for devices advertising the AcouSense service UUID
- When a known device (MAC in `bluetooth_devices`) is detected: mark as `PRESENT` with current timestamp
- Pass presence data to SyncManager for aggregation
- After the SyncManager triggers a sync: connect to the target device's GATT server, write the sync payload to the designated characteristic, disconnect

**Characteristic UUIDs**:

| Characteristic | UUID | Permission |
|---|---|---|
| Sync Payload Write | `0xAC01` | Write |
| Device Registration | `0xAC02` | Read + Write |

**Files**: `bluetooth_manager.h`, `bluetooth_manager.cpp`

---

### 3.7 SyncManager

**Responsibility**: Implement the 5-minute presence-based exposure cycle and prepare personalized sync payloads.

**Behavior**:
1. Every **5 minutes**, collect all `noise_logs` rows from the past 5 minutes
2. For each device marked `PRESENT` in that window:
   - Compute: `avg_noise`, `peak_noise`, `exposure_classification`
   - Build sync payload struct
   - Pass to BluetoothManager to transmit
3. Reset presence flags after sync
4. Disconnections happen automatically; BluetoothManager handles reconnect logic

**Sync Payload Struct**:
```c
struct SyncPayload {
    uint8_t  device_id[6];   // ESP32 MAC address
    char     user_id[32];    // From bluetooth_devices.user_identifier
    uint32_t ts_start;
    uint32_t ts_end;
    uint16_t avg_noise;
    uint16_t peak_noise;
    uint8_t  exposure_class; // 0=safe, 1=caution, 2=moderate, 3=high, 4=danger
};
```

**Exposure Classification Rules** (based on WHO guidelines):

| avg_noise (normalized) | Classification |
|---|---|
| < 40 | Safe (0) |
| 40–59 | Caution (1) |
| 60–74 | Moderate (2) |
| 75–89 | High (3) |
| ≥ 90 | Danger (4) |

**Files**: `sync_manager.h`, `sync_manager.cpp`

---

## 4. Shared Utilities

**`packet_protocol.h`**: Shared between this and the Arduino — defines packet structs, CRC8, build/parse functions. The ESP32 version is identical to the one on the Arduino.

---

## 5. File Structure

```
firmware/esp32/
├── acoustics_esp32.cpp         # Entry point: setup() + loop()
├── spi_master_manager.h/.cpp
├── sd_card_manager.h/.cpp
├── database_manager.h/.cpp
├── config_manager.h/.cpp
├── web_server.h/.cpp
├── bluetooth_manager.h/.cpp
├── sync_manager.h/.cpp
└── packet_protocol.h           # Shared packet definitions and CRC8
```

---

## 6. Dependencies

| Library | Purpose |
|---|---|
| `WiFi.h` | Wi-Fi connectivity |
| `ESPAsyncWebServer` | Non-blocking HTTP server |
| `AsyncTCP` | Required by ESPAsyncWebServer |
| `SD.h` | SD card file access |
| `SPI.h` | SPI master |
| `BLEDevice.h` | BLE Central operations |
| `ESP32-sqlite3` | SQLite on SD |

---

## 7. Validation Checklist

- [ ] SD card mounts and `acousense.db` is created on first boot
- [ ] Schema tables are initialized without error
- [ ] SPI polling receives valid 0x01 Audio Report from Arduino
- [ ] Audio reports are written to `noise_logs` table
- [ ] Config is loaded from DB on boot and pushed to Arduino on start
- [ ] `/api/status` returns valid JSON
- [ ] `/api/readings` returns rows from DB
- [ ] `/api/config` POST updates DB and pushes to Arduino
- [ ] BLE scan detects an advertising Android test device
- [ ] Sync payload is transmitted and received correctly
- [ ] Module failures (SD, BT, Wi-Fi) are isolated and reflected in `/api/status`
