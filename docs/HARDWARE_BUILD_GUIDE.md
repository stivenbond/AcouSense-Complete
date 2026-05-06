# AcouSense — Hardware Build Guide

## Overview

This guide provides step-by-step wiring instructions for assembling the AcouSense hardware module. Follow it sequentially. Do not power anything until all wiring described in a section is complete and double-checked.

**⚠️ CRITICAL: Read Section 2 (Voltage Safety) before touching any wire.**

---

## 1. Component List

| # | Component | Notes |
|---|---|---|
| 1 | ESP32-D2 development board | 30-pin or 38-pin variant |
| 2 | Arduino Nano V3 | ATmega328P, USB-B micro |
| 3 | Sound sensor module (microphone) | KY-038 or similar; has analog OUT pin |
| 4 | 16×2 I2C LCD module | With PCF8574 I2C backpack (address 0x27 or 0x3F) |
| 5 | SD card module (Arduino-compatible) | SPI interface, 3.3V or 5V compatible |
| 6 | Passive buzzer | NOT active buzzer — must be driven by PWM |
| 7 | Logic level shifter (bi-directional) | 4-channel BSS138-based (e.g. SparkFun BOB-12009) |
| 8 | Breadboard (full-size) or PCB | For prototyping |
| 9 | Jumper wires (male-male) | 20+ short wires, preferably < 10 cm |
| 10 | USB cable for Arduino Nano | USB-A to Mini-B |
| 11 | USB cable for ESP32 | USB-A to Micro-B or USB-C depending on board |
| 12 | Decoupling capacitors | 1× 100nF ceramic, 1× 10µF electrolytic |
| 13 | Regulated power supply | 5V/2A for Arduino rail, 3.3V LDO for ESP32 rail, or bench supply |

---

## 2. Voltage Safety — Read First

| Controller | Logic Level | GPIO Tolerance |
|---|---|---|
| Arduino Nano V3 | **5V** | 5V |
| ESP32-D2 | **3.3V** | **NOT 5V tolerant** |

**Connecting the Arduino's 5V outputs directly to the ESP32's 3.3V GPIO pins will permanently destroy the ESP32.**

The four SPI lines between the Arduino and ESP32 must pass through the logic level shifter without exception:
- **MOSI** (ESP32 → Arduino): 3.3V → 5V
- **MISO** (Arduino → ESP32): 5V → 3.3V ← most critical, would damage ESP32 directly
- **SCLK** (ESP32 → Arduino): 3.3V → 5V
- **CS/SS** (ESP32 → Arduino): 3.3V → 5V

---

## 3. Power Architecture

Build your power rails on the breadboard before connecting any components.

### Rails

| Rail | Voltage | Powers |
|---|---|---|
| Rail A (red) | 5V | Arduino Nano VIN/5V, LCD, Sound sensor, Buzzer |
| Rail B (blue) | GND | All components — shared common ground |
| Rail C (orange) | 3.3V | ESP32 3V3, SD card module VCC |

### Connections for Power Rails

| From | To | Wire Color |
|---|---|---|
| 5V supply (+) | Breadboard Rail A (+) | Red |
| 5V supply (−) | Breadboard Rail B (−) | Black |
| 3.3V LDO output | Breadboard Rail C (+) | Orange |
| Arduino Nano `5V` | Breadboard Rail A (+) | Red |
| Arduino Nano `GND` | Breadboard Rail B (−) | Black |
| ESP32 `3V3` | Breadboard Rail C (+) | Orange |
| ESP32 `GND` | Breadboard Rail B (−) | Black |

> All GND connections share Rail B — this is mandatory. Every module listed below must also connect its GND pin to Rail B.

---

## 4. Decoupling Capacitors

Place these before connecting the SD card module. Poor decoupling is the #1 cause of SD card initialization failures.

| Capacitor | Value | Placement |
|---|---|---|
| C1 | 100nF ceramic | Between SD module VCC and GND pins, placed as physically close to the pins as possible |
| C2 | 10µF electrolytic | Between Breadboard Rail C (+) and Rail B (−), near the 3.3V LDO output |

> Pour the 3.3V rail on the breadboard, place C2 first, then C1 at the SD module footprint before inserting the module.

---

## 5. Logic Level Shifter Wiring

The level shifter has two sides:
- **LV side**: Connected to ESP32 (3.3V logic)
- **HV side**: Connected to Arduino (5V logic)

### Level Shifter Power Connections

| Level Shifter Pin | Connects To | Rail |
|---|---|---|
| `LV` (3.3V reference) | Breadboard Rail C (+) | 3.3V |
| `GND` (LV side) | Breadboard Rail B (−) | GND |
| `HV` (5V reference) | Breadboard Rail A (+) | 5V |
| `GND` (HV side) | Breadboard Rail B (−) | GND |

### Level Shifter Signal Connections

| Channel | LV Side (ESP32) | HV Side (Arduino) | Signal |
|---|---|---|---|
| Channel 1 | ESP32 `GPIO23` | Arduino `D11` | MOSI |
| Channel 2 | ESP32 `GPIO19` | Arduino `D12` | MISO |
| Channel 3 | ESP32 `GPIO18` | Arduino `D13` | SCLK |
| Channel 4 | ESP32 `GPIO5` | Arduino `D10` | CS (Slave Select) |

---

## 6. Microphone Sound Sensor Module

| Sound Sensor Pin | Connects To | Notes |
|---|---|---|
| `VCC` | Breadboard Rail A (+) 5V | |
| `GND` | Breadboard Rail B (−) | |
| `AO` (Analog Out) | Arduino Nano `A0` | Main signal — analog voltage proportional to sound level |
| `DO` (Digital Out) | Not connected | Unused — we use analog output only |

---

## 7. I2C LCD (16×2 with PCF8574 Backpack)

| LCD Pin | Connects To | Notes |
|---|---|---|
| `VCC` | Breadboard Rail A (+) 5V | |
| `GND` | Breadboard Rail B (−) | |
| `SDA` | Arduino Nano `A4` | I2C data |
| `SCL` | Arduino Nano `A5` | I2C clock |

> The I2C address of the LCD backpack is either `0x27` or `0x3F` depending on the manufacturer. Run the I2C scanner in `firmware/tests/test_lcd.cpp` to detect the correct address before uploading the main firmware.

---

## 8. Passive Buzzer

| Buzzer Pin | Connects To | Notes |
|---|---|---|
| `+` (positive) | Arduino Nano `D9` | PWM output — tone() drives this pin |
| `−` (negative) | Breadboard Rail B (−) | Must be GND, not 5V |

> Use a **passive** buzzer only. An active buzzer has its own internal oscillator and cannot be controlled by PWM frequency. It will make a fixed tone regardless of what you send.

---

## 9. SD Card Module (ESP32 Side)

The SD card module connects to the ESP32 on a **separate SPI bus** from the Arduino.

| SD Module Pin | Connects To | Notes |
|---|---|---|
| `VCC` | Breadboard Rail C (+) 3.3V | Some SD modules accept 5V — check your module's datasheet |
| `GND` | Breadboard Rail B (−) | |
| `MOSI` | ESP32 `GPIO13` | |
| `MISO` | ESP32 `GPIO12` | |
| `SCK` | ESP32 `GPIO14` | |
| `CS` | ESP32 `GPIO15` | |

> If your SD module has a built-in 3.3V regulator and accepts 5V VCC, you may power it from Rail A instead. Use a multimeter to measure the VCC-to-GND voltage on the module after powering on — it should read exactly 3.3V on the SD card side.

---

## 10. Complete Pin Reference Table

### Arduino Nano V3 — All Pin Assignments

| Arduino Pin | Connected To | Function |
|---|---|---|
| `5V` | Rail A (+) | Power output |
| `GND` | Rail B (−) | Common ground |
| `A0` | Sound sensor `AO` | Microphone analog input |
| `A4` | LCD `SDA` | I2C data |
| `A5` | LCD `SCL` | I2C clock |
| `D9` | Buzzer `+` | PWM buzzer control |
| `D10` | Level shifter HV4 → ESP32 GPIO5 | SPI Slave Select (SS) |
| `D11` | Level shifter HV1 → ESP32 GPIO23 | SPI MOSI |
| `D12` | Level shifter HV2 → ESP32 GPIO19 | SPI MISO |
| `D13` | Level shifter HV3 → ESP32 GPIO18 | SPI SCLK |

### ESP32-D2 — All Pin Assignments

| ESP32 Pin | Connected To | Function |
|---|---|---|
| `3V3` | Rail C (+) | Power output |
| `GND` | Rail B (−) | Common ground |
| `GPIO5` | Level shifter LV4 → Arduino D10 | SPI CS for Arduino |
| `GPIO18` | Level shifter LV3 → Arduino D13 | SPI SCLK (Arduino bus) |
| `GPIO19` | Level shifter LV2 → Arduino D12 | SPI MISO (Arduino bus) |
| `GPIO23` | Level shifter LV1 → Arduino D11 | SPI MOSI (Arduino bus) |
| `GPIO12` | SD module `MISO` | SPI MISO (SD bus) |
| `GPIO13` | SD module `MOSI` | SPI MOSI (SD bus) |
| `GPIO14` | SD module `SCK` | SPI SCLK (SD bus) |
| `GPIO15` | SD module `CS` | SPI CS for SD card |

---

## 11. Wiring Sequence (Step-by-Step)

Follow this order to minimize errors. Power off completely between sections.

### Step 1 — Power Rails (Power Off)
1. Set up breadboard with Rail A (5V), Rail B (GND), Rail C (3.3V)
2. Do not connect power supply yet
3. Place decoupling capacitors C1 and C2

### Step 2 — Arduino Nano (Power Off)
4. Seat Arduino Nano on breadboard
5. Connect `5V` → Rail A, `GND` → Rail B

### Step 3 — Logic Level Shifter (Power Off)
6. Seat level shifter on breadboard
7. Connect `LV` → Rail C, `HV` → Rail A, both `GND` pins → Rail B
8. Connect LV side channels to ESP32 GPIOs (do NOT connect ESP32 yet — just the LV wires)
9. Connect HV side channels to Arduino D10, D11, D12, D13

### Step 4 — ESP32 (Power Off)
10. Seat ESP32 on breadboard
11. Connect `3V3` → Rail C, `GND` → Rail B
12. Connect GPIO pins to level shifter LV side (as per table above)
13. Connect GPIO12/13/14/15 to SD module

### Step 5 — SD Card Module (Power Off)
14. Connect SD module `VCC` → Rail C, `GND` → Rail B
15. Connect MOSI/MISO/SCK/CS to ESP32 GPIOs (Section 9)
16. Place C1 (100nF) across SD module VCC–GND leads

### Step 6 — Microphone (Power Off)
17. Connect sensor `VCC` → Rail A, `GND` → Rail B
18. Connect `AO` → Arduino A0

### Step 7 — LCD (Power Off)
19. Connect LCD `VCC` → Rail A, `GND` → Rail B
20. Connect `SDA` → Arduino A4, `SCL` → Arduino A5

### Step 8 — Buzzer (Power Off)
21. Connect buzzer `+` → Arduino D9, `−` → Rail B

### Step 9 — First Power-On
22. Inspect all wiring against the tables in Sections 5–10
23. Confirm no wires are bridging adjacent pins incorrectly
24. Connect 3.3V supply (Rail C) first and measure with multimeter — should read 3.30 ± 0.05V
25. Connect 5V supply (Rail A) and measure — should read 5.0 ± 0.1V
26. Connect USB to Arduino Nano → upload `test_lcd.cpp` → verify LCD displays text
27. Upload `test_microphone.cpp` → blow on mic → verify ADC values change in Serial Monitor
28. Upload `test_buzzer.cpp` → verify audible tone
29. Connect USB to ESP32 → upload `test_sdcard.cpp` → verify SD mount and file I/O in Serial Monitor

---

## 12. Pre-Firmware Checklist

Before flashing any production firmware, confirm all of these:

- [ ] Shared GND confirmed across all modules (measure continuity with multimeter)
- [ ] 3.3V rail measures stable under ESP32 + SD card load
- [ ] 5V rail measures stable under Arduino + LCD + Buzzer + Mic load
- [ ] Level shifter installed; LV/HV references connected correctly
- [ ] LCD I2C address detected by scanner sketch
- [ ] Microphone ADC reads varying values with sound input
- [ ] Buzzer produces tone on D9 PWM
- [ ] SD card mounts and file write/read verified
- [ ] No ESP32 GPIO gets more than 3.3V measured with multimeter (especially MISO line)

---

## 13. Troubleshooting

| Symptom | Likely Cause | Fix |
|---|---|---|
| ESP32 gets very hot | 5V applied to GPIO | Disconnect immediately; check level shifter wiring |
| LCD shows nothing | Wrong I2C address | Run I2C scanner sketch; adjust address in code |
| LCD shows blocks | Contrast pot not adjusted | Turn the small blue potentiometer on the I2C backpack |
| SD card not detected | Unstable 3.3V or missing decoupling | Add capacitors; check 3.3V rail stability |
| Microphone always reads 0 or max | Sensor not powered or wrong pin | Confirm VCC at sensor, confirm wire on A0 |
| SPI communication fails | Missing shared GND or level shifter not powered | Check GND continuity; check LV/HV reference pins |
| Buzzer silent | Active buzzer used instead of passive | Replace with passive buzzer |
