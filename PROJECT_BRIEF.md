# Noise Exposure Monitoring Ecosystem — Complete System Specification

## 1. Project Overview

This project is a distributed embedded system designed to monitor environmental noise, provide immediate local alerts, maintain historical records, synchronize user-specific exposure data to nearby mobile devices, and generate long-term exposure reports with health recommendations.

The system is intentionally divided between two microcontrollers: an ESP32-D2 and an Arduino Nano V3. The Arduino handles deterministic, real-time hardware operations such as microphone sampling, LCD updates, and buzzer alerts, while the ESP32 manages storage, networking, Bluetooth communication, configuration persistence, and web service hosting.

This architecture ensures that time-sensitive sensor operations remain isolated from higher-level tasks such as database operations, Bluetooth scanning, and web server requests. The goal is to create a reliable and scalable exosystem composed of embedded firmware, a local web platform, an Android application, and a clear hardware schematic foundation.

The device monitors ambient sound levels continuously and calculates minimum, maximum, and average noise values over time. These readings are displayed locally, stored historically, and selectively synchronized to users only when they are physically present near the monitored environment. This allows personal exposure reporting instead of generic room-level statistics.

The final system consists of:

- Arduino Nano firmware
- ESP32 firmware
- Web dashboard
- Android application
- Bluetooth synchronization system
- Local SQLite historical storage
- Hardware schematic and communication contracts

The most important design principle is that all communication contracts and responsibilities must be finalized before implementation begins.

---

# 2. Hardware Architecture

## 2.1 Physical Components

The hardware system consists of the following components:

### Main Controllers

- ESP32-D2
- Arduino Nano V3

### Peripheral Modules

- Arduino microphone sound sensor module
- I2C 16x2 LCD display with backpack driver
- SD card Arduino module
- Passive buzzer

The ESP32 acts as the system master and is responsible for storage, networking, Bluetooth operations, and web hosting. The Arduino Nano acts as the real-time sensor controller and user feedback unit.

The communication between the two controllers is established using SPI, where the ESP32 is the SPI master and the Arduino Nano is the SPI slave.

---

## 2.2 Hardware Responsibility Split

### Arduino Nano Responsibilities

The Arduino is responsible for all operations that require deterministic timing and continuous local responsiveness.

It continuously reads the microphone module and processes raw analog values into usable sound level metrics. These values are displayed live on the LCD and evaluated against configurable thresholds to determine alert severity.

Depending on the current noise level, the Arduino activates the buzzer using PWM with different sound patterns. These patterns are configurable and are received from the ESP32.

Every ten seconds, the Arduino aggregates audio readings and sends a packet to the ESP32 containing the minimum, maximum, and average noise levels.

Its responsibilities include:

- continuous microphone sampling
- rolling statistical analysis
- LCD live status display
- buzzer activation with configurable PWM patterns
- SPI slave communication
- runtime configuration application

### ESP32 Responsibilities

The ESP32 handles all non-deterministic and high-level operations.

It receives readings from the Arduino, stores them in a historical SQLite database located on the SD card, serves the local web dashboard over Wi-Fi, manages Bluetooth discovery and synchronization with nearby Android devices, and stores system configuration.

It is also responsible for pushing configuration changes to the Arduino over SPI whenever thresholds or buzzer behaviors are modified from the web interface.

Its responsibilities include:

- SPI master communication
- SD card storage
- SQLite database management
- web dashboard hosting
- Wi-Fi connectivity
- Bluetooth discovery and synchronization
- paired device management
- configuration persistence
- periodic mobile sync operations

---

# 3. SPI Communication Contract

## 3.1 Communication Methodology

SPI is the backbone of the embedded architecture and must be treated as the primary system contract.

The ESP32 is the SPI master and initiates all communication. The Arduino Nano is the SPI slave and only responds when addressed.

The protocol must use fixed-size binary packets instead of JSON or text serialization. Human-readable formats create unnecessary overhead, poor timing guarantees, and parsing complexity that are unacceptable for embedded systems.

Communication must include packet framing, validation, acknowledgments, and corruption detection.

Initial SPI clock speed should remain conservative at 500 kHz to prioritize stability during development and debugging.

---

## 3.2 Standard Packet Format

Every SPI transmission follows the same packet structure:

| Field | Size |
|---|---:|
| Start Byte | 1 byte |
| Packet Type | 1 byte |
| Payload Length | 1 byte |
| Payload | Variable |
| CRC8 | 1 byte |
| End Byte | 1 byte |

The start byte is fixed to `0xAA`, and the end byte is fixed to `0x55`.

CRC8 is mandatory to detect packet corruption and prevent invalid configuration or sensor data from entering the system.

---

## 3.3 Packet Types

### Type 0x01 — Audio Report Packet

This packet is sent from the Arduino to the ESP32 every ten seconds.

It contains the aggregated sound statistics for the previous reporting interval.

Payload structure:

| Field | Type | Size |
|---|---|---:|
| timestamp | uint32 | 4 |
| min_level | uint16 | 2 |
| max_level | uint16 | 2 |
| avg_level | uint16 | 2 |
| alert_level | uint8 | 1 |

The `alert_level` field indicates the currently active threshold category such as low, medium, or high severity.

---

### Type 0x02 — Configuration Packet

This packet is sent from the ESP32 to the Arduino whenever configuration changes are applied.

It updates threshold values and buzzer behavior rules.

Payload structure:

| Field | Type |
|---|---|
| low_threshold | uint16 |
| medium_threshold | uint16 |
| high_threshold | uint16 |
| pattern_low | uint8 |
| pattern_medium | uint8 |
| pattern_high | uint8 |

This allows the Arduino firmware to remain generic while operational behavior is controlled externally.

---

### Type 0x03 — ACK Packet

This packet confirms successful packet reception.

It is bidirectional.

Payload structure:

| Field | Type |
|---|---|
| acknowledged_packet_type | uint8 |
| status | uint8 |

This is required to ensure configuration reliability.

---

### Type 0x04 — Heartbeat Packet

Heartbeat packets are used to verify that both controllers remain synchronized and operational.

This helps detect communication failures, stalled firmware states, or disconnected hardware.

---

# 4. Arduino Firmware Specification

## 4.1 Functional Overview

The Arduino firmware is responsible for deterministic sensing and immediate local feedback.

Its architecture should be modular rather than written as a single monolithic loop.

The firmware should avoid `delay()` entirely and instead rely on `millis()` scheduling for non-blocking execution. This is mandatory because sensor reading, LCD refresh, buzzer control, and SPI responsiveness must all operate concurrently.

---

## 4.2 Firmware Modules

### Sensor Manager

This module continuously reads the microphone sensor using analog input and normalizes raw values into meaningful sound levels.

It maintains rolling calculations for:

- minimum noise level
- maximum noise level
- average noise level

These values are refreshed continuously and finalized every ten seconds for transmission.

---

### LCD Manager

The LCD displays real-time values and system state.

Typical display content includes:

- live current sound level
- current alert severity
- system connection state
- communication status

The LCD must remain responsive and readable without excessive refresh flickering.

---

### Buzzer Manager

The buzzer manager evaluates the current noise level against configured thresholds and applies the correct PWM alert pattern.

Different patterns should represent different severity levels such as:

- intermittent short beep for low warning
- repeating medium pulse for moderate warning
- rapid aggressive alarm for high warning

Patterns are not hardcoded permanently and must be configurable through SPI updates.

---

### SPI Slave Manager

This module handles incoming configuration packets and outgoing report packets.

It validates framing, CRC, and packet type before applying configuration changes.

Robust packet rejection is required to prevent invalid state transitions.

---

### Configuration Manager

This module stores runtime configuration currently received from the ESP32 and exposes it to other firmware modules.

Threshold changes must apply immediately without requiring restart.

---

# 5. ESP32 Firmware Specification

## 5.1 Functional Overview

The ESP32 firmware is the system coordinator.

It manages persistence, networking, Bluetooth synchronization, and device administration.

Its design must separate transport logic from application logic to prevent failures in one subsystem from affecting others.

If development speed is critical, Arduino framework is acceptable. If stability and long-term maintainability are prioritized, ESP-IDF is preferable.

---

## 5.2 Firmware Modules

### SPI Master Manager

This module polls the Arduino, requests periodic reports, sends configuration packets, and validates acknowledgments.

It also handles heartbeat verification and retry logic.

---

### Database Manager

This module writes incoming sensor reports to SQLite stored on the SD card.

It must ensure:

- write integrity
- safe transaction handling
- corruption prevention
- clean recovery after unexpected power loss

Historical persistence is one of the project’s core requirements.

---

### SD Card Manager

This module initializes storage, verifies mount state, handles file access, and protects against SD failures.

Many Arduino SD modules are unstable with poor wiring, so software validation is essential.

---

### Configuration Manager

This stores thresholds and behavior rules persistently and exposes them to both the web dashboard and Arduino synchronization layer.

Configuration changes must survive power cycles.

---

### Web Server

The ESP hosts a local dashboard accessible through Wi-Fi.

This allows configuration, visualization, CSV export, and system inspection without requiring cloud infrastructure.

---

### Bluetooth Manager

This module handles discovery, pairing, reconnection, and selective synchronization with Android devices.

It is one of the most complex parts of the system and must be designed around scalability rather than permanent connections.

---

### Sync Manager

This prepares five-minute exposure summaries and transmits them only to users confirmed to be physically present nearby.

This is how personal exposure tracking is achieved.

---

# 6. Bluetooth Synchronization Specification

## 6.1 Design Goal

The system should not simply store room noise data. It should determine what portion of that noise exposure belongs to specific users.

This is achieved by proximity-based Bluetooth synchronization.

Only users physically present near the device should receive exposure records.

This prevents false reporting and supports accurate personal recommendations.

---

## 6.2 Discovery Strategy

Maintaining permanent Bluetooth connections to many devices is unrealistic and inefficient.

The correct design is a periodic burst synchronization model.

The ESP32 continuously performs discovery for known application devices using a dedicated BLE service UUID rather than relying only on MAC addresses.

This allows trusted identification of phones with the installed app.

---

## 6.3 Five-Minute Presence Cycle

The synchronization cycle works as follows:

1. The ESP scans nearby devices
2. It identifies previously paired devices
3. It marks them as currently present (“checked in”)
4. It continues discovery while collecting room data
5. After five minutes, it aggregates exposure readings
6. It reconnects only to checked-in devices
7. It sends the five-minute report
8. It disconnects again

Connections are therefore temporary and periodic rather than continuous.

This design allows realistic support for approximately thirty users.

---

## 6.4 Mobile Sync Payload

Each synchronization event sends:

- device ID
- user ID
- timestamp start
- timestamp end
- average noise level
- peak noise level
- exposure classification

The phone stores only the periods during which the user was actually present.

This creates personalized exposure history instead of duplicated global room logs.

---

# 7. SQLite Database Specification

The ESP32 stores historical data locally using SQLite on the SD card.

This enables persistence without requiring external servers.

The schema must be minimal, stable, and query-efficient.

---

## 7.1 noise_logs Table

This table stores every ten-second aggregated reading received from the Arduino.

| Field | Type |
|---|---|
| id | INTEGER |
| timestamp | INTEGER |
| min_level | INTEGER |
| max_level | INTEGER |
| avg_level | INTEGER |
| alert_level | INTEGER |

This is the primary historical dataset for both web visualization and mobile synchronization.

---

## 7.2 configuration Table

This stores the currently active system thresholds and buzzer patterns.

| Field | Type |
|---|---|
| id | INTEGER |
| low_threshold | INTEGER |
| medium_threshold | INTEGER |
| high_threshold | INTEGER |
| pattern_low | INTEGER |
| pattern_medium | INTEGER |
| pattern_high | INTEGER |

This ensures persistence across reboot cycles.

---

## 7.3 bluetooth_devices Table

This stores known mobile users and their pairing history.

| Field | Type |
|---|---|
| id | INTEGER |
| mac_address | TEXT |
| user_identifier | TEXT |
| last_seen | INTEGER |

This supports presence detection and reporting consistency.

---

# 8. Web Dashboard Specification

## 8.1 Platform Choice

SvelteKit is recommended over SolidJS due to faster implementation speed, clearer routing structure, and simpler production deployment for this project.

The web dashboard is intended for administrators and device owners rather than end users.

It provides operational control and historical inspection.

---

## 8.2 Required Features

### Live Dashboard

The dashboard must display:

- current sound level
- active alert state
- connection status
- SD card status
- Arduino communication health
- Bluetooth synchronization status

This provides immediate operational visibility.

---

### Historical Data Viewer

Users must be able to browse:

- daily history
- weekly history
- monthly history

Data should be shown both as graphs and tables.

Filtering and search are required.

---

### CSV Export

Users must be able to export readings as CSV for external analysis and academic reporting.

This is often required for project evaluation.

---

### Configuration Panel

Administrators must be able to edit:

- threshold levels
- buzzer patterns
- reporting behavior

Configuration should be stored and optionally pushed immediately to the Arduino.

---

### Device Management

The dashboard should show:

- known paired users
- last seen timestamps
- synchronization history
- active nearby users

This improves debugging and trust in Bluetooth operations.

---

## 8.3 REST API Contract

The web dashboard should consume the following endpoints:

- `GET /api/readings`
- `GET /api/readings/export`
- `GET /api/config`
- `POST /api/config`
- `GET /api/devices`
- `GET /api/status`

These endpoints should remain stable and versionable.

---

# 9. Android Application Specification

## 9.1 Purpose

The Android app is not a controller for the device. It is a personal exposure analysis platform.

Its purpose is to collect user-specific noise exposure periods and transform them into understandable health reports.

The distinction is important: the web dashboard manages the device, while the mobile app serves the user.

---

## 9.2 Core Features

### Pairing and Registration

The user installs the app and pairs once with the ESP32 ecosystem.

After pairing, presence detection becomes automatic.

---

### Exposure Tracking

The app stores:

- daily exposure summaries
- weekly exposure reports
- monthly trends
- peak event history

This provides a longitudinal health perspective.

---

### Visualization

Charts and summaries should show:

- average daily exposure
- repeated high-risk events
- exposure duration above WHO recommendations

Users need interpretation, not just raw numbers.

---

### Recommendation Engine

The app uses local inference with Gemma 4 e2b to generate advice based on WHO noise exposure guidelines.

This should remain local and offline.

No cloud dependency should exist.

---

## 9.3 AI Recommendation Input

The model should receive:

- daily exposure totals
- weekly exposure patterns
- repeated threshold violations
- high-risk peak events
- user exposure classification

---

## 9.4 AI Recommendation Output

The model should generate short actionable recommendations such as:

- reduce exposure duration
- use hearing protection
- leave high-noise environments temporarily
- improve recovery periods between exposure events

The system should produce simple health guidance rather than medical diagnosis.

---

# 10. Hardware Schematic Guidance

## 10.1 Shared Ground

All controllers and modules must share a common ground reference.

This is mandatory and non-negotiable.

Without shared ground, SPI and analog readings become unreliable.

---

## 10.2 Voltage Compatibility

This is the most dangerous hardware issue.

Arduino Nano operates at 5V.

ESP32 operates at 3.3V.

Direct SPI connection without level shifting can permanently damage the ESP32.

A logic level shifter is strongly recommended.

At minimum, proper resistor division must protect lines where the Arduino outputs to the ESP32.

This must be handled before prototyping.

---

## 10.3 SD Card Stability

Many SD modules fail due to:

- long jumper wires
- unstable power supply
- insufficient decoupling
- voltage mismatch

Use short wiring and proper decoupling capacitors.

Storage reliability is critical for this project.

---

## 10.4 Power Strategy

A single stable regulated power source is preferred.

Avoid mixed USB sources and unstable adapters during development.

Inconsistent grounding creates false debugging problems.

---

# 11. Development Methodology

Implementation order is critical.

Incorrect sequencing causes major delays.

The correct development order is:

1. Hardware validation
2. SPI packet communication
3. Arduino standalone firmware
4. ESP32 standalone firmware
5. SQLite persistence layer
6. Web dashboard
7. Bluetooth synchronization
8. Android application
9. AI recommendation engine

The project must not begin with the web interface or Android app.
The entire ecosystem depends on the SPI contract and hardware reliability first.
That is the true foundation of the project.