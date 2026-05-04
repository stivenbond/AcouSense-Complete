# Arduino Nano Firmware Specification

## 1. Overview

The Arduino Nano firmware is a deterministic, real-time embedded application responsible for continuous microphone sampling, statistical analysis, LCD status display, buzzer control, and SPI slave communication with the ESP32 master.

The firmware must be **non-blocking throughout**. All scheduling relies on `millis()` comparison — `delay()` is prohibited.

---

## 2. Architecture

The firmware follows a flat, cooperative multitasking model using a single `loop()` function that dispatches to independent module update functions on each iteration.

```
loop()
├── SensorManager::update()
├── LCDManager::update()
├── BuzzerManager::update()
├── SPISlaveManager::update()
└── ConfigManager (passive — read-only consumer)
```

Each module is self-contained with its own state variables and timing registers. No module waits for another.

---

## 3. Module Specifications

### 3.1 SensorManager

**Responsibility**: Continuous ADC reading, rolling statistics computation, and 10-second report preparation.

**Behavior**:
- Sample microphone on analog pin A0 every **50ms** (20 Hz)
- Normalize raw ADC value (0–1023) to a 0–100 scale representing sound level
- Maintain a rolling window of samples for the current 10-second interval:
  - `min_level`: lowest normalized value seen
  - `max_level`: highest normalized value seen
  - `avg_level`: running sum / sample count
- After 10 seconds, finalize the report struct and set `reportReady = true`
- Reset the rolling window for the next interval

**Data output**:
```c
struct AudioReport {
    uint32_t timestamp;   // millis() / 1000
    uint16_t min_level;
    uint16_t max_level;
    uint16_t avg_level;
    uint8_t  alert_level; // determined by ConfigManager thresholds
};
```

**Files**: `sensor_manager.h`, `sensor_manager.cpp`

---

### 3.2 LCDManager

**Responsibility**: Real-time display of sound level and system state on the 16×2 I2C LCD.

**Display Layout**:
```
Line 1: [Level: 078 dB  ]
Line 2: [ALERT: LOW  OK ]
```

**Behavior**:
- Refresh LCD every **500ms** to avoid flicker while keeping data current
- Display `current_level` from SensorManager on line 1
- Display current `alert_level` as text ("NONE", "LOW", "MED", "HIGH") on line 2
- Display ESP32 connection state: "OK" / "ERR" / "---" in the last 3 characters of line 2
- If SPI communication has been inactive for > 30 seconds, display "NO ESP" on line 2

**Files**: `lcd_manager.h`, `lcd_manager.cpp`

---

### 3.3 BuzzerManager

**Responsibility**: Evaluate current alert level and apply the correct configurable PWM buzzer pattern.

**Behavior**:
- Read `alert_level` from SensorManager and cross-reference with pattern rules from ConfigManager
- Apply pattern non-blocking using internal state machine and `millis()` timing

**Pattern State Machine**:

| Pattern ID | Waveform |
|---|---|
| 0x00 | Off (no output) |
| 0x01 | ON 100ms → OFF 2900ms (single beep every 3s) |
| 0x02 | ON 100ms → OFF 200ms → ON 100ms → OFF 1600ms (double pulse) |
| 0x03 | ON 80ms → OFF 80ms continuously (rapid alarm) |

**PWM**: Use `tone()` on pin D9 at a fixed frequency (e.g. 2000Hz) with the above on/off timing. If no active alert, call `noTone()`.

**Files**: `buzzer_manager.h`, `buzzer_manager.cpp`

---

### 3.4 SPISlaveManager

**Responsibility**: Handle all SPI slave communication — receive configuration packets and transmit audio report packets on demand.

**Behavior**:
- Use SPI interrupt (`SPI_STC_vect`) to receive bytes into a ring buffer
- In `update()`, process any complete packets from the buffer:
  - Validate start byte (`0xAA`)
  - Read packet type and payload length
  - Read payload bytes
  - Read and validate CRC8
  - Validate end byte (`0x55`)
  - On success: dispatch to handler; send ACK with `status=0x00`
  - On CRC failure: send ACK with `status=0x01`; discard packet
  - On unknown type: send ACK with `status=0x02`; discard packet
- When a 0x04 Heartbeat is received, queue an ACK response
- When a report is polled (implied by CS low + no payload), if `reportReady == true`, transmit 0x01 Audio Report Packet and clear `reportReady`

**Files**: `spi_slave_manager.h`, `spi_slave_manager.cpp`

---

### 3.5 ConfigManager

**Responsibility**: Store and expose runtime configuration received via SPI.

**Behavior**:
- Holds the current threshold and buzzer pattern config in RAM
- Provides getter functions used by SensorManager and BuzzerManager
- No EEPROM persistence on Arduino — thresholds are re-sent by ESP32 on boot via 0x02 config packet

**Default values** (before first config packet):
```c
uint16_t low_threshold    = 400;  // ADC normalized units
uint16_t medium_threshold = 600;
uint16_t high_threshold   = 800;
uint8_t  pattern_low      = 0x01;
uint8_t  pattern_medium   = 0x02;
uint8_t  pattern_high     = 0x03;
```

**Files**: `config_manager.h`, `config_manager.cpp`

---

## 4. Timing Summary

| Task | Interval | Trigger |
|---|---|---|
| Microphone ADC sample | 50 ms | `millis()` |
| LCD refresh | 500 ms | `millis()` |
| Audio report finalization | 10 s | `millis()` |
| SPI rx buffer poll | Every `loop()` | Continuous |
| Buzzer pattern tick | Pattern-dependent | Internal state machine |

---

## 5. File Structure

```
firmware/arduino/
├── acoustics_nano.ino        # Entry point: setup() + loop()
├── sensor_manager.h/.cpp
├── lcd_manager.h/.cpp
├── buzzer_manager.h/.cpp
├── spi_slave_manager.h/.cpp
├── config_manager.h/.cpp
└── packet_protocol.h         # Shared: packet structs, CRC8, constants
```

---

## 6. Dependencies

| Library | Purpose |
|---|---|
| `Wire.h` | I2C for LCD |
| `LiquidCrystal_I2C` | LCD driver |
| `SPI.h` | SPI slave |

All libraries available via Arduino Library Manager.

---

## 7. Validation Checklist

- [ ] ADC reads non-zero values when mic is exposed to sound
- [ ] `min`, `max`, `avg` reset correctly after each 10-second window
- [ ] LCD displays correct values without flicker
- [ ] Buzzer produces correct PWM pattern per alert level
- [ ] SPI correctly receives 0x02 config packet and updates thresholds
- [ ] SPI correctly transmits 0x01 audio report when polled
- [ ] CRC mismatch causes rejection without crashing
- [ ] `delay()` is absent from the entire codebase
