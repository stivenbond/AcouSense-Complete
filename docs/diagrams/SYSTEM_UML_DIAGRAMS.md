# AcouSense — System UML Diagrams

This document provides a comprehensive technical overview of the AcouSense ecosystem, covering hardware topology, software architecture, and data flow.

---

## 1. Hardware Deployment Diagram

This diagram illustrates the physical components and their electrical/wireless interconnections. It highlights the dual-microcontroller architecture and the transition to UART communication.

```mermaid
graph TB
    subgraph Env ["&nbsp;&nbsp;Physical Environment&nbsp;&nbsp;"]
        Sound["fa:fa-volume-up Ambient Sound (Waves)"]
    end

    subgraph Nano ["Arduino Nano V3 (5V)"]
        direction TB
        Mic["fa:fa-microphone Mic Sensor"]
        LCD["fa:fa-desktop I2C LCD"]
        Buzzer["fa:fa-bell Buzzer (PWM)"]
        Nano_Core["fa:fa-microchip ATmega328P Core"]
    end

    subgraph ESP ["ESP32-D2 (3.3V)"]
        direction TB
        ESP_Core["fa:fa-microchip ESP32 Dual-Core"]
        SD_Card["fa:fa-hdd SD Card Module"]
        WiFi["fa:fa-wifi Wi-Fi Radio"]
        BLE["fa:fa-bluetooth-b Bluetooth Low Energy"]
    end

    subgraph LLS ["&nbsp;&nbsp;Logic Level Shifter&nbsp;&nbsp;"]
        StepDown["5V → 3.3V"]
        StepUp["3.3V → 5V"]
    end

    subgraph Users ["User Interfaces"]
        Phone["fa:fa-mobile Android App"]
        Dashboard["fa:fa-globe Web Dashboard"]
    end

    %% Connections
    Sound --- Mic
    Mic --- Nano_Core
    Nano_Core --- LCD
    Nano_Core --- Buzzer

    Nano_Core -- "TX/RX" --- LLS
    LLS -- "UART" --- ESP_Core
    
    ESP_Core --- SD_Card
    ESP_Core -. "HTTP/Web" .-> Dashboard
    ESP_Core -. "GATT Sync" .-> Phone

    %% Styling
    classDef hardware fill:#ffffff,stroke:#2c3e50,stroke-width:2px,color:#2c3e50,font-weight:bold;
    classDef arduino fill:#e3f2fd,stroke:#1565c0,stroke-width:2px,color:#0d47a1;
    classDef esp fill:#fff8e1,stroke:#f57f17,stroke-width:2px,color:#e65100;
    classDef level fill:#f3e5f5,stroke:#7b1fa2,stroke-width:2px,color:#4a148c;
    classDef user fill:#e8f5e9,stroke:#2e7d32,stroke-width:2px,color:#1b5e20;
    
    class Nano_Core,Mic,LCD,Buzzer arduino;
    class ESP_Core,SD_Card,WiFi,BLE esp;
    class StepDown,StepUp,LLS level;
    class Phone,Dashboard user;
    class Env,Sound hardware;
```

---

## 2. Software Component Diagram

The following diagram represents the modular decomposition of the software ecosystem and the interfaces between subsystems.

```mermaid
graph LR
    subgraph Firmware ["Firmware Layer (C++)"]
        direction TB
        A[Arduino: Real-time Sensing]
        E[ESP32: System Coordination]
    end

    subgraph Storage ["Persistence Layer"]
        DB[(SQLite on SD)]
    end

    subgraph App ["Mobile App (Kotlin/Compose)"]
        direction TB
        M[UI & Domain Logic]
        AI[Gemma AI Inference]
    end

    subgraph Web ["Web Layer (SvelteKit)"]
        W[Dashboard UI]
        API[ESP32 REST API]
    end

    %% Interactions
    A -- "PacketProtocol (UART)" --> E
    E -- "Filesystem SPI" --> DB
    E -- "HTTP/JSON" --- API
    API --- W
    E -- "BLE GATT" --- M
    M --- AI

    %% Styling
    classDef layer fill:#f5f5f5,stroke:#333,stroke-width:1px,stroke-dasharray: 5 5;
    classDef comp fill:#ffffff,stroke:#2c3e50,stroke-width:2px;
    classDef db fill:#ffffff,stroke:#2c3e50,stroke-width:2px;
    
    class A,E,M,AI,W,API comp;
    class DB db;
    class Firmware,Storage,App,Web layer;
```

---

## 3. Logical Class Diagram (Core Abstractions)

Focuses on the "Manager" architectural pattern used in the firmware and the data contracts.

```mermaid
classDiagram
    direction LR
    class PacketHeader {
        +uint8_t startByte
        +uint8_t type
        +uint8_t length
    }

    class AudioReport {
        +uint32_t timestamp
        +uint16_t leq_dbz
        +uint16_t min_dbz
        +uint16_t max_dbz
        +uint8_t alert_level
    }

    class SensorManager {
        <<Arduino>>
        -Microphone mic
        -RollingStats stats
        +update()
        +getReport() AudioReport
    }

    class SyncManager {
        <<ESP32>>
        -BluetoothManager ble
        -DatabaseManager db
        +processFiveMinCycle()
        +pushToUsers()
    }

    class RecommendationEngine {
        <<Android>>
        -GemmaModel model
        +analyze(history) Recommendation
    }

    AudioReport --|> PacketHeader : Implements
    SensorManager ..> AudioReport : Generates
    SyncManager ..> AudioReport : Consumes
    RecommendationEngine ..> SyncManager : Syncs Data
```

---

## 4. System Sequence Diagram (Data Lifecycle)

Traces a noise measurement from the physical world to the user's phone, illustrating the periodic synchronization model.

```mermaid
sequenceDiagram
    autonumber
    participant Env as Environment
    participant Nano as Arduino Nano
    participant ESP as ESP32
    participant SD as SD Card (SQLite)
    participant App as Android App

    Note over Env,Nano: Continuous Sampling
    Env->>Nano: Sound Waves (Vibration)
    Nano->>Nano: Statistical Integration [dB(Z)]
    
    Note over Nano,ESP: Every 10 Seconds
    Nano->>ESP: UART Packet: AudioReport (Type 0x01)
    ESP->>SD: Transaction: INSERT INTO noise_logs
    
    Note over ESP,App: 5-Minute Proximity Check
    ESP->>App: BLE Advertisement Scan
    App-->>ESP: Identity Confirmation (UUID Match)
    
    Note over ESP,App: Sync Triggered
    ESP->>ESP: Aggregate Period Exposure
    ESP->>App: GATT Write: ExposurePayload
    App->>App: Local DB Persistence
    App->>App: Execute Gemma Inference
    App-->>App: UI: "Actionable Health Alert"
```
