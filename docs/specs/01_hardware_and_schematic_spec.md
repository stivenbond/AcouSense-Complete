# Hardware & Schematic Specification

## 1. Overview

The AcouSense hardware layer consists of two microcontrollers (ESP32-D2 and Arduino Nano V3) and a set of peripheral modules. This document specifies the physical wiring rules, voltage compatibility strategy, and component responsibilities.

---

## 2. Component List

| Component | Role |
|---|---|
| ESP32-D2 | System master: storage, networking, Bluetooth, web hosting |
| Arduino Nano V3 | Real-time sensor controller: microphone, LCD, buzzer |
| Microphone Sound Sensor Module | Analog audio input |
| I2C 16×2 LCD with backpack | Live status display |
| SD Card Module (Arduino) | SD card access via SPI |
| Passive Buzzer | Audio alert output |
| Logic Level Shifter (bi-directional) | Voltage isolation between Arduino (5V) and ESP32 (3.3V) |
| Decoupling Capacitors (100nF + 10µF) | SD card and power line stabilization |

---

## 3. Pinout Assignments

### ESP32 SPI Master Pins (to Level Shifter HV side)

| Signal | ESP32 GPIO |
|---|---|
| MOSI | GPIO 23 |
| MISO | GPIO 19 |
| SCLK | GPIO 18 |
| CS (Arduino) | GPIO 5 |

### Arduino Nano SPI Slave Pins (from Level Shifter LV side)

| Signal | Arduino Pin |
|---|---|
| MOSI | D11 |
| MISO | D12 |
| SCLK | D13 |
| SS | D10 |

### Arduino Nano Peripheral Pins

| Peripheral | Arduino Pin |
|---|---|
| Microphone Analog Out | A0 |
| LCD SDA (I2C) | A4 |
| LCD SCL (I2C) | A5 |
| Buzzer PWM Out | D9 |

### ESP32 SD Card Pins (separate SPI bus)

| Signal | ESP32 GPIO |
|---|---|
| SD MOSI | GPIO 13 |
| SD MISO | GPIO 12 |
| SD SCLK | GPIO 14 |
| SD CS | GPIO 15 |

---

## 4. Voltage Compatibility Rules

- Arduino Nano: 5V logic
- ESP32: 3.3V logic (NOT 5V tolerant on GPIO)
- **A bi-directional logic level shifter is mandatory on all SPI lines between the two controllers.**
- The MISO line (Arduino → ESP32) is the critical path: the Arduino outputs 5V which would damage the ESP32 permanently without isolation.
- Minimum acceptable alternative: voltage divider (10kΩ + 20kΩ) on MOSI and SCLK from Arduino output lines — but a dedicated level shifter is strongly preferred.

---

## 5. Shared Ground

All components must share a common GND rail:
- Arduino GND → ESP32 GND
- LCD GND → Arduino GND
- SD Module GND → ESP32 GND
- Buzzer GND → Arduino GND
- Level Shifter GND (LV side) → Arduino GND
- Level Shifter GND (HV side) → Arduino GND (shared)

**Failure to share ground will result in unreliable SPI and corrupted analog readings.**

---

## 6. SD Card Stability Guidelines

- Use short jumper wires (< 10 cm) between ESP32 and SD module
- Place a 100nF decoupling capacitor at the SD module VCC pin
- Place a 10µF bulk capacitor near the power input
- Verify 3.3V supply stability with a multimeter before first boot
- Avoid USB hubs or laptop USB ports as the sole power source during testing

---

## 7. Power Strategy

| Scenario | Recommendation |
|---|---|
| Development | Single regulated 5V + 3.3V bench supply |
| Prototype | USB to Arduino Nano with a separate 3.3V LDO for ESP32 |
| Production | Single regulated adapter with distinct LDO rails per MCU |

- Never power both MCUs from different USB sources without bridging grounds.
- Inconsistent grounding creates false SPI timing errors and corrupted ADC readings.

---

## 8. SPI Clock Speed

- Initial development: **500 kHz** (conservative, prioritizes stability)
- After full validation: can increase to 1–4 MHz if timing margins allow
- Do not increase clock speed until all CRC validation passes consistently

---

## 9. Validation Checklist

- [ ] Shared GND confirmed across all modules
- [ ] Level shifter installed and verified with logic analyzer
- [ ] 3.3V supply stable under load (ESP32 + SD card)
- [ ] 5V supply stable under load (Arduino + LCD + Buzzer)
- [ ] SD card detects and mounts successfully
- [ ] I2C LCD responds at address 0x27 or 0x3F
- [ ] Microphone ADC returns varying values when sound is applied
- [ ] Buzzer responds to PWM signal at 9
