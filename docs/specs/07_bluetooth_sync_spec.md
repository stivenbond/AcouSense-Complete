# Bluetooth Synchronization Specification

## 1. Overview

The Bluetooth synchronization system is the mechanism that transforms room-level noise data into personal exposure records. It determines which users were physically present near the device during a given period and delivers only their relevant exposure summary to their Android device.

This is achieved through a **periodic burst synchronization model** using BLE (Bluetooth Low Energy). Connections are intentionally short-lived and happen on a 5-minute cycle — not continuous streams.

---

## 2. BLE Role Assignments

| Device | BLE Role |
|---|---|
| ESP32 | Central (Scanner + Client) |
| Android App | Peripheral (Advertiser + GATT Server) |

The Android device advertises a known AcouSense service UUID at all times when the app is running in the foreground or background. The ESP32 scans for this UUID and initiates connections.

---

## 3. BLE Service & Characteristic Definitions

**Service UUID**: `0000AC00-0000-1000-8000-00805F9B34FB`

| Characteristic | UUID | Properties | Description |
|---|---|---|---|
| Sync Payload | `0000AC01-0000-1000-8000-00805F9B34FB` | Write Without Response | ESP32 writes sync payload to this characteristic |
| Device Registration | `0000AC02-0000-1000-8000-00805F9B34FB` | Read + Write | Android writes user_identifier on pairing; ESP32 reads |
| Acknowledge | `0000AC03-0000-1000-8000-00805F9B34FB` | Notify | Android notifies ESP32 when sync payload is successfully stored |

---

## 4. Five-Minute Presence Cycle

### Cycle Timeline

```
[t=0]      Cycle begins
           ESP32 starts BLE scan

[t=0..5m]  Continuous scanning
           → When AcouSense device detected:
             - Lookup MAC in bluetooth_devices
             - If known: mark PRESENT, log ts_start and ts_end
             - If unknown: ignore (not registered)

[t=5m]     Cycle end trigger fires:
           → Aggregate noise_logs for past 5 minutes
           → For each PRESENT device:
             - Build SyncPayload
             - Connect to device GATT server
             - Write SyncPayload to 0xAC01
             - Wait for ACK notify (timeout: 5 seconds)
             - Disconnect
           → Reset all PRESENT flags

[t=5m+]    New cycle begins immediately
```

### Concurrency Rules

- Connect to at most **one device at a time** per sync cycle
- The scan continues running during connection/write operations if the ESP32 BLE stack allows dual mode; otherwise scan is paused during connection
- Connection timeout: 10 seconds per device
- Write timeout: 5 seconds after connection
- If a device fails to connect or acknowledge: log error; skip; continue to next device

---

## 5. Sync Payload Binary Format

The sync payload is transmitted as a fixed-size binary struct (not JSON) over the BLE characteristic write. This keeps the payload compact and predictable.

```c
struct SyncPayload {
    uint8_t  device_id[6];      // ESP32 MAC address (6 bytes)
    uint8_t  user_id[32];       // user_identifier, null-padded (32 bytes)
    uint32_t ts_start;          // Period start (Unix timestamp, 4 bytes)
    uint32_t ts_end;            // Period end (Unix timestamp, 4 bytes)
    uint16_t avg_noise;         // Average normalized noise (0-100), 2 bytes
    uint16_t peak_noise;        // Maximum normalized noise (0-100), 2 bytes
    uint8_t  exposure_class;    // 0=safe, 1=caution, 2=moderate, 3=high, 4=danger
    uint8_t  reserved;          // Padding to align to even byte
};
// Total: 6 + 32 + 4 + 4 + 2 + 2 + 1 + 1 = 52 bytes
```

---

## 6. Pairing / Registration Flow

### First-Time Pairing (Android → ESP32)

1. User opens AcouSense app → navigates to "Pair Device"
2. App starts advertising AcouSense service UUID
3. App also starts an active BLE scan looking for ESP32 by service UUID (`0xAC00`)
4. User selects the detected ESP32 device in the app
5. App connects to the ESP32's GATT server (ESP32 temporarily runs as Peripheral for pairing only)
6. App writes `user_identifier` (UUID string) to `0xAC02` characteristic
7. ESP32 reads `user_identifier` + records MAC address → inserts into `bluetooth_devices`
8. App displays "Paired successfully"
9. From this point forward, presence detection is automatic

**Note**: The ESP32 runs as both Central (normal operation) and Peripheral (pairing only). This is a temporary role swap during the one-time pairing handshake.

---

## 7. Presence Detection Logic

```
onDeviceDiscovered(mac, serviceUUIDs):
    if AcouSense_UUID not in serviceUUIDs:
        return
    device = lookupDB(mac)
    if device is null:
        return  // unknown device, skip
    device.markPresent(currentTimestamp)
    updateDB(mac, last_seen=currentTimestamp)
```

A device is considered **present** if it was detected at least once in the current 5-minute window. Signal strength (RSSI) filtering may be added in v2 to limit range — not required for v1.

---

## 8. Exposure Classification

Applied per sync payload generation using normalized noise values (0–100 scale):

| Range | Class ID | Label | WHO Reference |
|---|---|---|---|
| avg_noise < 40 | 0 | Safe | Below WHO ambient threshold |
| 40 ≤ avg_noise < 60 | 1 | Caution | Moderate background |
| 60 ≤ avg_noise < 75 | 2 | Moderate | Elevated, prolonged exposure risky |
| 75 ≤ avg_noise < 90 | 3 | High | WHO recommends limiting duration |
| avg_noise ≥ 90 | 4 | Danger | Immediate risk, WHO limit exceeded |

---

## 9. Scalability Target

- v1 design supports approximately **30 concurrent users** (30 devices in `bluetooth_devices`)
- Each 5-minute sync cycle: 30 sequential connections × (10s timeout + 5s write) = ~7.5 minutes maximum
- If sync time exceeds 5-minute cycle, overlap resolution strategy: extend cycle, skip slowest/furthest device
- v2 can introduce parallel connection handling if the ESP32 BLE stack supports it

---

## 10. Error Handling

| Condition | Behavior |
|---|---|
| Device present but fails to connect | Log error; skip device; continue cycle |
| Write to characteristic times out | Log error; disconnect; skip device |
| ACK not received within 5 seconds | Log warning; assume received; continue |
| DB query fails for 5-minute window | Skip sync cycle; log error |
| ESP32 BLE stack crash | Reinitialize BLE; resume scanning |

---

## 11. Validation Checklist

- [ ] ESP32 correctly scans and detects Android device advertising AcouSense UUID
- [ ] Unknown devices are ignored during scan
- [ ] Pairing writes `user_identifier` and MAC to DB correctly
- [ ] Presence detection marks device PRESENT during 5-minute window
- [ ] SyncPayload is built with correct values after 5-minute aggregation
- [ ] BLE write to `0xAC01` succeeds and Android receives payload
- [ ] Android stores received SyncPayload locally
- [ ] Disconnection after sync is clean and scan resumes
- [ ] Failed connection is logged and cycle continues
- [ ] 30-device concurrent scenario is estimated within time budget
