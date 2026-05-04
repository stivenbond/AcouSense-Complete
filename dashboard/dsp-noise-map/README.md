# DSP Noise Map

Real-time urban noise mapping system with edge DSP and Inverse Distance Weighting (IDW) spatial interpolation.

## Architecture

```text
┌──────────────┐         ┌───────────┐         ┌───────────────┐         ┌─────────────┐
│ ESP32 Sensor │         │   MQTT    │         │ Python Server │         │ HTML Client │
│ (I2S + FFT)  ├─ MQTT ─►│  Broker   ├─ MQTT ─►│  (SQLite3)    ├─ HTTP ─►│ (Leaflet)   │
└──────────────┘  JSON   └───────────┘  JSON   └───────────────┘  JSON   └─────────────┘
```

## Directory Structure

```text
dsp-noise-map/
├── firmware/
│   └── main.cpp      # ESP32 PlatformIO Sketch
├── backend/
│   └── server.py     # Flask backend and MQTT subscriber
├── frontend/
│   └── index.html    # Leaflet-based dashboard
└── README.md         # You are here
```

## Firmware (ESP32)

Edge device calculating equivalent continuous sound level (Leq) using an INMP441 I2S microphone.

### PlatformIO Configuration

Add the following to your `platformio.ini`:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps =
    kosme/arduinoFFT @ ^2.0.1
    bblanchon/ArduinoJson @ ^6.21.3
    knolleary/PubSubClient @ ^2.8
    mikalhart/TinyGPSPlus @ ^1.0.3
```

### Wiring Table

| Component | ESP32 Pin | Details           |
|-----------|-----------|-------------------|
| INMP441   | GPIO 32   | I2S SD (Data)     |
| INMP441   | GPIO 25   | I2S WS (LRCLK)    |
| INMP441   | GPIO 26   | I2S SCK (BCLK)    |
| INMP441   | L/R       | GND (Left Channel)|
| GPS       | GPIO 16   | UART RX (Serial2) |
| GPS       | GPIO 17   | UART TX (Serial2) |

### Calibration
To calibrate, compare the output SPL reading via Serial console to a certified Class 1 or Class 2 sound level meter. Edit `MIC_OFFSET_DB` in `main.cpp` to apply a trim offset. Adjust `MIC_REF_DB` and `MIC_REF_AMPL` if using an acoustic calibrator (e.g. 94dB at 1kHz).

## Backend

Python server that aggregates sensor data and exposes a REST API.

### Installation & Execution

Install dependencies:
```bash
pip install paho-mqtt flask flask-cors
```

Run the server:
```bash
python backend/server.py
```

### MQTT Broker (Mosquitto)
You will need an MQTT Broker.
- **Ubuntu/Debian:** `sudo apt install mosquitto mosquitto-clients`
- **macOS:** `brew install mosquitto`

### REST API Endpoints

| Method | Endpoint         | Description                                              |
|--------|------------------|----------------------------------------------------------|
| GET    | `/api/readings`  | Raw readings. Params: `minutes`, `sensor_id`             |
| GET    | `/api/latest`    | Most recent reading for every active sensor              |
| GET    | `/api/heatmap`   | Generates IDW grid. Params: `minutes`, `grid_n`, `power` |
| GET    | `/api/stats`     | Global summary statistics (sensors, count, min, max, avg)|

## Frontend

The frontend is a single `index.html` file.
Just open `frontend/index.html` in your browser. 
If the backend is not running at `localhost:5000`, the app will automatically fall back into **Demo Mode** showing simulated heatmap data in Tirana.

### Controls

| Control        | Description                                  |
|----------------|----------------------------------------------|
| Time Window    | Filters readings to a specific recent window |
| IDW Power      | Adjusts falloff gradient of IDW interpolation|
| Grid Res       | Grid resolution points (Low=20, Med=40)      |
| Refresh Button | Triggers an immediate API redraw             |

## Noise Zones Table

Based on standard acoustic health guidelines:

| Zone      | dB(A) Range | Description                                | Source Guideline    |
|-----------|-------------|--------------------------------------------|---------------------|
| 🔴 Danger | > 85 dB(A)  | Dangerous · Permanent hearing damage       | NIOSH / OSHA Limits |
| 🟡 Caution| 70-85 dB(A) | Cautionary · Tiring; short exposure only   | EPA General Limits  |
| 🟢 Safe   | < 70 dB(A)  | Safe · Comfortable for long-term use       | WHO Comm. Noise     |

## DSP Pipeline Detail

1. **A-Weighting:** The FFT splits the audio into frequency bins. For each bin, its amplitude is converted to decibels. We apply a linear interpolation from standard IEC 61672-1 A-weighting tables to correct the human perception sensitivity.
2. **Leq Calculation:** Equivalent continuous level ($L_{eq}$) is computed over `LEQ_WINDOW_SEC`. 
   $$ L_{eq} = 10 \cdot \log_{10} \left( \frac{1}{N} \sum_{i=1}^{N} 10^{L_i / 10} \right) $$
3. **IDW Interpolation:** Inverse Distance Weighting uses Haversine distance. Crucially, acoustic decibels cannot be averaged strictly linearly. They must be transformed to linear power space ($10^{dB/10}$) prior to applying spatial weights, and mapped back to logarithmic space.

## Extending
- **Multi-floor layers:** Add z-index or altitude tracking for overlapping 3D noise sources.
- **Kriging:** Use `pykrige` instead of IDW for better geostatistical covariance interpolation.
- **Threshold Alerts:** Add an MQTT publisher or webhook alert when levels exceed 85dB for a specific timeframe.
- **NTP Time:** Implement ESP32 native SNTP to assign exact hardware ISO timestamps instead of simple server side ingestion timestamps.
