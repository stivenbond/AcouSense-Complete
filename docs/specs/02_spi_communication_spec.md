# SPI Communication Contract Specification

## 1. Overview

SPI is the backbone of the AcouSense embedded architecture and defines the primary contract between the ESP32 (master) and the Arduino Nano (slave). This document specifies the packet format, packet types, framing rules, CRC calculation, and behavioral guarantees that both firmware implementations must honor exactly.

---

## 2. Physical Layer

| Parameter | Value |
|---|---|
| Role (ESP32) | Master |
| Role (Arduino Nano) | Slave |
| Clock Speed (initial) | 500 kHz |
| Mode | SPI Mode 0 (CPOL=0, CPHA=0) |
| Bit Order | MSB first |
| Voltage Isolation | Required (logic level shifter) |

---

## 3. Standard Packet Format

Every SPI transmission, in both directions, follows this fixed-envelope structure:

| Field | Size | Value / Notes |
|---|---:|---|
| Start Byte | 1 byte | Always `0xAA` |
| Packet Type | 1 byte | See Section 4 |
| Payload Length | 1 byte | Number of payload bytes (0–255) |
| Payload | Variable | Type-specific, see Section 4 |
| CRC8 | 1 byte | CRC8 over [Packet Type + Payload Length + Payload] |
| End Byte | 1 byte | Always `0x55` |

**Minimum packet size**: 5 bytes (empty payload)
**Maximum payload size**: 255 bytes

---

## 4. CRC8 Calculation

- Algorithm: CRC-8 (Dallas/Maxim, polynomial `0x31`)
- Input: bytes from `Packet Type` through the last payload byte (inclusive)
- The Start Byte and End Byte are excluded from CRC computation
- Both sides must use identical CRC implementations

```c
uint8_t crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0x00;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x31;
            else            crc <<= 1;
        }
    }
    return crc;
}
```

---

## 5. Packet Types

### 0x01 — Audio Report Packet (Arduino → ESP32)

Sent every **10 seconds** by the Arduino, triggered by the ESP32 polling via CS line assertion.

Payload:

| Field | C Type | Size | Notes |
|---|---|---:|---|
| `timestamp` | `uint32_t` | 4 | Seconds since Arduino boot (millis()/1000) |
| `min_level` | `uint16_t` | 2 | Minimum ADC-normalized sound level in interval |
| `max_level` | `uint16_t` | 2 | Maximum ADC-normalized sound level in interval |
| `avg_level` | `uint16_t` | 2 | Average ADC-normalized sound level in interval |
| `alert_level` | `uint8_t` | 1 | 0=none, 1=low, 2=medium, 3=high |

Total payload: **11 bytes**
Full packet: **16 bytes**

---

### 0x02 — Configuration Packet (ESP32 → Arduino)

Sent whenever an administrator changes thresholds or buzzer patterns via the web dashboard.

Payload:

| Field | C Type | Size | Notes |
|---|---|---:|---|
| `low_threshold` | `uint16_t` | 2 | ADC level that triggers low alert |
| `medium_threshold` | `uint16_t` | 2 | ADC level that triggers medium alert |
| `high_threshold` | `uint16_t` | 2 | ADC level that triggers high alert |
| `pattern_low` | `uint8_t` | 1 | Buzzer pattern ID for low severity |
| `pattern_medium` | `uint8_t` | 1 | Buzzer pattern ID for medium severity |
| `pattern_high` | `uint8_t` | 1 | Buzzer pattern ID for high severity |

Total payload: **9 bytes**
Full packet: **14 bytes**

Buzzer pattern IDs:

| ID | Pattern |
|---|---|
| 0x00 | Silent (no buzzer) |
| 0x01 | Single short beep every 3 seconds |
| 0x02 | Double pulse every 2 seconds |
| 0x03 | Rapid continuous alarm |

---

### 0x03 — ACK Packet (Bidirectional)

Acknowledges successful receipt of the preceding packet. Must be sent by the receiver after CRC validation passes.

Payload:

| Field | C Type | Size | Notes |
|---|---|---:|---|
| `acknowledged_type` | `uint8_t` | 1 | Packet type being acknowledged |
| `status` | `uint8_t` | 1 | 0x00=OK, 0x01=CRC fail, 0x02=unknown type |

Total payload: **2 bytes**
Full packet: **7 bytes**

---

### 0x04 — Heartbeat Packet (ESP32 → Arduino)

Sent by the ESP32 every **30 seconds** to confirm synchronized operation. The Arduino replies with an ACK (0x03) with `acknowledged_type = 0x04`.

Payload: **empty (0 bytes)**
Full packet: **5 bytes**

If the ESP32 receives no ACK within 500 ms of a heartbeat, it logs a communication warning. After 3 consecutive failures, it marks the Arduino as unresponsive.

---

## 6. Communication Flow

### Normal Report Cycle (every 10 seconds)

```
ESP32 asserts CS
ESP32 sends: [Poll Request - can be 0x04 heartbeat]
Arduino replies: [0x01 Audio Report Packet]
ESP32 sends: [0x03 ACK, status=OK]
ESP32 de-asserts CS
```

### Configuration Push

```
ESP32 asserts CS
ESP32 sends: [0x02 Configuration Packet]
Arduino replies: [0x03 ACK, status=OK or CRC fail]
If CRC fail: ESP32 retries up to 3 times
ESP32 de-asserts CS
```

### Heartbeat

```
ESP32 asserts CS
ESP32 sends: [0x04 Heartbeat]
Arduino replies: [0x03 ACK, acknowledged_type=0x04]
ESP32 de-asserts CS
```

---

## 7. Error Handling Rules

| Condition | Required Behavior |
|---|---|
| CRC mismatch on receiver | Discard packet; send ACK with status=0x01 |
| Unknown packet type | Discard packet; send ACK with status=0x02 |
| Malformed framing (bad start/end bytes) | Discard entire buffer; reset parser state |
| No ACK within 500ms | Sender retries up to 3 times, then logs error |
| 3 consecutive heartbeat failures | ESP32 marks Arduino as unresponsive; web dashboard reflects status |

---

## 8. Implementation Contract

Both firmware sides must implement:

- `parsePacket(buffer, len)` — validates framing, CRC, type
- `buildPacket(type, payload, payloadLen, outBuffer)` — constructs a valid framed packet
- `sendACK(acknowledgedType, status)` — shorthand for building and sending a 0x03 packet
- A state machine (not a blocking loop) for packet reception

Neither side may use `delay()` during SPI operations.
