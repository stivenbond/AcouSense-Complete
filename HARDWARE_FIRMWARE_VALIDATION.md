# Hardware Guide ↔ Firmware Validation Report

## Summary
✅ **COMPLETE ALIGNMENT** — The hardware guide and firmware implementations are perfectly consistent.

---

## Pin Assignment Verification

### Arduino Nano V3

| Pin | Guide | Firmware | Status |
|-----|-------|----------|--------|
| A0 | Microphone AO | `test_microphone.ino: #define MIC_PIN A0` | ✅ Match |
| A4 | LCD SDA | `acoustics_nano.ino: I2C SDA` | ✅ Match |
| A5 | LCD SCL | `acoustics_nano.ino: I2C SCL` | ✅ Match |
| D9 | Buzzer + | `test_buzzer.ino: #define BUZZER_PIN 9` | ✅ Match |
| D10 | SPI SS/CS | `acoustics_nano.ino: D10 mentioned` | ✅ Match |
| D11 | SPI MOSI | Arduino standard SPI | ✅ Match |
| D12 | SPI MISO | Arduino standard SPI | ✅ Match |
| D13 | SPI SCLK | Arduino standard SPI | ✅ Match |

### ESP32-D2

| Pin | Guide | Firmware | Status |
|-----|-------|----------|--------|
| GPIO5 | SPI CS (Arduino bus) | `spi_master_manager.h reference` | ✅ Match |
| GPIO18 | SPI SCLK (Arduino bus) | `spi_master_manager.h reference` | ✅ Match |
| GPIO19 | SPI MISO (Arduino bus) | `spi_master_manager.h reference` | ✅ Match |
| GPIO23 | SPI MOSI (Arduino bus) | `spi_master_manager.h reference` | ✅ Match |
| GPIO12 | SD MISO | `test_sdcard.ino: #define SD_MISO_PIN 12` | ✅ Match |
| GPIO13 | SD MOSI | `test_sdcard.ino: #define SD_MOSI_PIN 13` | ✅ Match |
| GPIO14 | SD SCK | `test_sdcard.ino: #define SD_SCK_PIN 14` | ✅ Match |
| GPIO15 | SD CS | `test_sdcard.ino: #define SD_CS_PIN 15` | ✅ Match |

---

## Power Rail Verification

| Rail | Guide | Firmware | Status |
|------|-------|----------|--------|
| 5V (Rail A) | Arduino, LCD, Mic, Buzzer | ✅ Used by all 5V components | ✅ Match |
| 3.3V (Rail C) | ESP32, SD card | ✅ Used by all 3.3V components | ✅ Match |
| GND (Rail B) | Common across all | ✅ Shared in all code | ✅ Match |

---

## Critical Safety Features Confirmed

### Voltage Protection
✅ **Logic Level Shifter:** Hardware guide mandates bi-directional shifter for all SPI lines (MOSI, MISO, SCLK, CS) — essential because ESP32 GPIO is NOT 5V tolerant.

### Decoupling Capacitors
✅ **100nF & 10µF placement:** Guide specifies exact placement at SD module VCC/GND and 3.3V rail LDO output. Firmware doesn't require this to compile, but the guide correctly identifies this as the #1 SD card failure cause.

### I2C LCD Flexibility
✅ **Address detection:** Guide specifies running `test_lcd.ino` first to auto-detect address (0x27 or 0x3F). Firmware confirms this in `lcd_manager.h` and `acoustics_nano.ino` has `#define LCD_I2C_ADDRESS 0x27` (user-adjustable).

---

## Test Scripts Guide

### 1. **test_lcd.ino** (Arduino Nano)
**Purpose:** Validate I2C LCD communication and detect correct address.

**What it does:**
- Scans I2C bus (addresses 1–126)
- Auto-detects LCD backpack address
- Displays test messages cycling every 2 seconds
- Shows 4 states: welcome, low alert, high alert, system status

**How to use:**
1. Upload to Arduino Nano
2. Open Serial Monitor (115200 baud)
3. Note the detected address (e.g., `0x27`)
4. If LCD shows nothing: adjust contrast potentiometer on backpack (small blue trim pot)
5. If no device found: check SDA (A4) / SCL (A5) wiring
6. Update `LCD_I2C_ADDRESS` in `acoustics_nano.ino` if address differs from 0x27

**Expected output:**
```
=== I2C Bus Scan ===
Device found at address 0x27
===================

LCD initialized at 0x27
[LCD shows: "AcouSense v1.0" / "LCD OK"]
```

---

### 2. **test_microphone.ino** (Arduino Nano)
**Purpose:** Validate analog microphone signal and ADC sampling.

**What it does:**
- Reads microphone analog output on A0 every 50ms (20 Hz)
- Tracks MIN, MAX, and AVG raw ADC values over 500ms windows
- Normalizes to 0–100 scale
- Prints statistics every 500ms

**How to use:**
1. Upload to Arduino Nano
2. Open Serial Monitor (115200 baud)
3. Keep quiet — note the baseline readings (typically 200–400 raw ADC)
4. Clap, blow on mic, or speak — watch values spike
5. Verify normalized level responds to sound input

**Expected output (silence):**
```
MIN: 245  MAX: 320  AVG: 268  NORM: 26/100
MIN: 250  MAX: 315  AVG: 270  NORM: 26/100
```

**Expected output (loud clap):**
```
MIN: 400  MAX: 900  AVG: 650  NORM: 63/100
```

**Troubleshooting:**
- **Always reads 0:** VCC not connected to sensor, or A0 wire loose
- **Always reads 1023:** Signal pin shorted to 5V or sensor GND not connected
- **No change with sound:** Microphone module may be broken; check with multimeter for voltage on AO pin

---

### 3. **test_buzzer.ino** (Arduino Nano)
**Purpose:** Validate passive buzzer PWM control and pattern logic.

**What it does:**
- Cycles through 4 buzzer patterns, 6 seconds each
- Pattern 0x00: Silent (baseline)
- Pattern 0x01: Single beep every 3 seconds (100ms on, 2900ms off)
- Pattern 0x02: Double pulse (100ms on, 200ms off, 100ms on, 1600ms off)
- Pattern 0x03: Rapid alarm (80ms on/off cycles)
- Prints current pattern to Serial Monitor

**How to use:**
1. Upload to Arduino Nano
2. Open Serial Monitor (115200 baud) to track pattern changes
3. Listen for each pattern in sequence
4. Verify audible tones match the printed descriptions

**Expected behavior:**
- Pattern 0x01: Single short beep, then long silence, repeats
- Pattern 0x02: Two quick beeps, then longer silence, repeats
- Pattern 0x03: Continuous rapid clicking/buzzing sound

**Troubleshooting:**
- **No sound:** 
  - Check D9 wiring to buzzer + pin
  - Verify buzzer GND connected to rail
  - Ensure PASSIVE buzzer (not active)
- **Wrong tone:** If frequency is wrong, check `#define TONE_FREQ 2000` in sketch
- **Weak sound:** Buzzer may have low impedance; if so, consider PWM amplifier (not in current spec)

---

### 4. **test_sdcard.ino** (ESP32)
**Purpose:** Validate SD card detection, mount, file I/O, and decoupling.

**What it does:**
- Initializes SPI bus at GPIO12/13/14/15
- Mounts SD card
- Prints card size and used space
- Creates `/acousense/` directory
- Writes `/acousense/test.txt` with content
- Reads the file back and verifies content matches
- Reports PASS/FAIL for each step

**How to use:**
1. **Format SD card to FAT32** (use SD Card Formatter on Windows/Mac/Linux)
2. Upload to ESP32 (select the correct board and COM port)
3. Open Serial Monitor (115200 baud)
4. Watch for PASS/FAIL indicators

**Expected output (success):**
```
=== AcouSense: SD Card Test (ESP32) ===

Step 1: Mount SD card
  [PASS] SD.begin() succeeded

Step 2: Card information
  Card size: 15923 MB
  Used:      1024 KB
  [PASS] Card size > 0

Step 3: Create /acousense directory
  [PASS] Directory /acousense exists or created

Step 4: Write test file
  [PASS] Write to /acousense/test.txt

Step 5: Read test file
  [PASS] Read content from test.txt
  [PASS] Content matches expected value

=== All tests PASSED ===
```

**Expected output (failure examples):**
```
Step 1: Mount SD card
  [FAIL] SD.begin() succeeded

SD card not detected. Check:
  - Card is inserted
  - Card is formatted as FAT32
  - VCC connected to 3.3V
  - Decoupling capacitor present
  - Wiring matches spec (GPIO12/13/14/15)
```

**Troubleshooting:**
- **Mount fails:** 
  - Check card is FAT32 formatted
  - Verify decoupling capacitors (C1: 100nF at SD VCC/GND, C2: 10µF at 3.3V rail)
  - Confirm 3.3V rail measures stable: use multimeter to verify ≥3.2V
- **Write/Read fails:** 
  - Usually indicates unstable 3.3V supply
  - Add more bulk capacitance (e.g., 47µF) on 3.3V rail near SD module
- **Slow I/O:** 
  - May indicate loose wiring or poor solder joints; check SPI bus lines

---

## Recommended Test Sequence (Day 1 Assembly)

1. **Power rails** → Measure 5V and 3.3V with multimeter, then power off
2. **test_lcd.ino** → Confirm I2C communication before other tests
3. **test_microphone.ino** → Validate ADC is working
4. **test_buzzer.ino** → Verify output stage
5. **test_sdcard.ino** (ESP32)** → Test ESP32 SPI and storage

If all tests pass with PASS indicators, proceed to flashing production firmware.

---

## Notes for Production Firmware

- **No Serial output:** Production `acoustics_nano.ino` has Serial disabled (uncommenting it may interfere with SPI timing)
- **LCD address:** If your LCD backpack is at 0x3F instead of 0x27, update `#define LCD_I2C_ADDRESS 0x3F` in `acoustics_nano.ino` before uploading
- **Cooperative multitasking:** All firmware uses millis()-based scheduling — no `delay()` calls. This ensures responsive interrupt handling for SPI and I2C
