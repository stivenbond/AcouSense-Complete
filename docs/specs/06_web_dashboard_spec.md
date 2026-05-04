# Web Dashboard Specification

## 1. Overview

The AcouSense web dashboard is a local administrator interface served directly by the ESP32 over Wi-Fi. It provides operational visibility, historical data inspection, system configuration, and device management. It is not a cloud service — it runs entirely on the local network.

**Target users**: Device administrators and environment owners.  
**Framework**: SvelteKit  
**Deployment**: Static build output (`npm run build`) served from `/www/` on the SD card via `ESPAsyncWebServer`.

---

## 2. Technology Stack

| Layer | Technology |
|---|---|
| Framework | SvelteKit |
| Language | TypeScript |
| Styling | Vanilla CSS (custom design system) |
| Charts | Chart.js |
| HTTP Client | Native `fetch` API |
| Build Output | Static adapter (`@sveltejs/adapter-static`) |

**Why SvelteKit**: Minimal bundle size (critical for SD card hosting), fast build pipeline, simple routing, no server-side runtime needed (static export).

---

## 3. Page Structure & Routes

```
/                    → Live Dashboard
/history             → Historical Data Viewer
/config              → Configuration Panel
/devices             → Device Management
```

---

## 4. Pages

### 4.1 Live Dashboard (`/`)

**Purpose**: Immediate operational visibility into the current system state.

**Displayed Data**:

| Widget | Source | Update Interval |
|---|---|---|
| Current Sound Level | `GET /api/status` | Every 5 seconds |
| Active Alert State | `GET /api/status` | Every 5 seconds |
| Arduino Connection | `GET /api/status` | Every 5 seconds |
| SD Card Status | `GET /api/status` | Every 5 seconds |
| Wi-Fi RSSI | `GET /api/status` | Every 30 seconds |
| Bluetooth Sync Status | `GET /api/status` | Every 10 seconds |
| Last Sync Timestamp | `GET /api/status` | Every 30 seconds |

**UI Components**:
- Numeric gauge for current dB level (large, central)
- Color-coded alert badge: green (none), yellow (low), orange (medium), red (high)
- Status pill row: `ARDUINO ● ONLINE`, `SD ● OK`, `BT ● SCANNING`
- Mini sparkline chart: last 60 readings (10 minutes of history)

---

### 4.2 Historical Data Viewer (`/history`)

**Purpose**: Browse and analyze historical noise readings.

**Features**:
- Date range picker (daily / weekly / monthly presets + custom)
- Line chart showing `avg_level` over time (Chart.js)
- Overlay toggles: show `min_level`, `max_level`, alert threshold lines
- Paginated data table below chart: timestamp, min, max, avg, alert level
- Search/filter by alert level
- "Export CSV" button → triggers `GET /api/readings/export` with current date range

**API calls**:
```
GET /api/readings?from={ts}&to={ts}&limit={n}&offset={n}
GET /api/readings/export?from={ts}&to={ts}
```

---

### 4.3 Configuration Panel (`/config`)

**Purpose**: Edit thresholds and buzzer patterns; apply immediately to the Arduino.

**Form Fields**:

| Field | Type | Description |
|---|---|---|
| Low Threshold | Number input (0–100) | Noise level triggering low alert |
| Medium Threshold | Number input (0–100) | Noise level triggering medium alert |
| High Threshold | Number input (0–100) | Noise level triggering high alert |
| Buzzer Pattern (Low) | Select dropdown | Pattern 0–3 |
| Buzzer Pattern (Medium) | Select dropdown | Pattern 0–3 |
| Buzzer Pattern (High) | Select dropdown | Pattern 0–3 |

**Behavior**:
- On load: `GET /api/config` populates form
- On submit: `POST /api/config` with JSON body
- Server pushes 0x02 config packet to Arduino immediately
- Success/failure banner displayed after submission

**Validation**:
- `low_threshold < medium_threshold < high_threshold` enforced client-side
- Values must be in range 0–100

---

### 4.4 Device Management (`/devices`)

**Purpose**: View and manage known paired Bluetooth devices.

**Displayed Data**:
- Table of all rows from `bluetooth_devices`
- Columns: User Identifier, MAC Address, Last Seen (human-readable timestamp), Status (PRESENT / LAST SEEN X min ago)
- "Active Now" badge for devices seen within the last 5-minute sync window

**API calls**:
```
GET /api/devices
```

---

## 5. REST API Contract

All endpoints are served by the ESP32 at `http://{device-ip}/api/`.

### 5.1 `GET /api/status`

**Response**:
```json
{
  "arduino": "online" | "offline",
  "sd_card": "ok" | "error",
  "wifi_rssi": -65,
  "bt_status": "scanning" | "syncing" | "idle",
  "last_sync_ts": 1713526800,
  "last_reading": {
    "timestamp": 1713526780,
    "avg_level": 72,
    "max_level": 88,
    "alert_level": 2
  }
}
```

---

### 5.2 `GET /api/readings`

**Query params**: `from` (unix ts), `to` (unix ts), `limit` (default 200), `offset` (default 0)

**Response**:
```json
{
  "total": 8640,
  "limit": 200,
  "offset": 0,
  "data": [
    { "id": 1, "timestamp": 1713000000, "min_level": 30, "max_level": 75, "avg_level": 52, "alert_level": 1 }
  ]
}
```

---

### 5.3 `GET /api/readings/export`

**Query params**: `from`, `to` (optional)

**Response**: `text/csv` download
```
id,timestamp,min_level,max_level,avg_level,alert_level
1,1713000000,30,75,52,1
```

---

### 5.4 `GET /api/config`

**Response**:
```json
{
  "low_threshold": 40,
  "medium_threshold": 60,
  "high_threshold": 80,
  "pattern_low": 1,
  "pattern_medium": 2,
  "pattern_high": 3
}
```

---

### 5.5 `POST /api/config`

**Request body** (same shape as GET response, all fields required):
```json
{
  "low_threshold": 40,
  "medium_threshold": 65,
  "high_threshold": 85,
  "pattern_low": 1,
  "pattern_medium": 2,
  "pattern_high": 3
}
```

**Response**:
```json
{ "success": true, "arduino_synced": true }
```

---

### 5.6 `GET /api/devices`

**Response**:
```json
[
  {
    "id": 1,
    "mac_address": "AA:BB:CC:DD:EE:FF",
    "user_identifier": "user_abc123",
    "last_seen": 1713526800
  }
]
```

---

## 6. Design System

**Color palette**:
- Background: `#0f1117`
- Surface: `#1a1d27`
- Card: `#22263a`
- Accent: `#6c63ff` (primary)
- Alert Green: `#22c55e`
- Alert Yellow: `#facc15`
- Alert Orange: `#f97316`
- Alert Red: `#ef4444`
- Text primary: `#f0f0f5`
- Text secondary: `#8b8fa8`

**Typography**: `Inter` (Google Fonts CDN or local fallback)

**Design language**: Dark mode, glassmorphism cards, smooth hover transitions, subtle CSS animations on status indicators.

---

## 7. Build & Deployment

```bash
# In dashboard/
npm install
npm run build           # Outputs to dashboard/build/

# Copy to SD card
cp -r dashboard/build/* /Volumes/SD_CARD/www/
```

The ESP32 serves all files from `/www/` on the SD card as static assets. `index.html` is the fallback for all non-API routes (SPA routing).

---

## 8. File Structure

```
dashboard/
├── src/
│   ├── routes/
│   │   ├── +layout.svelte          # Global nav + styles
│   │   ├── +page.svelte            # Live Dashboard (/)
│   │   ├── history/+page.svelte
│   │   ├── config/+page.svelte
│   │   └── devices/+page.svelte
│   ├── lib/
│   │   ├── components/
│   │   │   ├── StatusCard.svelte
│   │   │   ├── AlertBadge.svelte
│   │   │   ├── NoiseGauge.svelte
│   │   │   ├── SparklineChart.svelte
│   │   │   └── DataTable.svelte
│   │   ├── api.ts                  # Typed fetch wrappers for all endpoints
│   │   └── types.ts                # Shared TypeScript interfaces
│   └── app.css                     # Global design system tokens
├── static/
├── svelte.config.js
├── vite.config.ts
└── package.json
```

---

## 9. Validation Checklist

- [ ] `npm run build` completes without errors
- [ ] Dashboard loads from ESP32 IP on same Wi-Fi network
- [ ] Live dashboard auto-refreshes without page reload
- [ ] Historical chart renders correctly with real data
- [ ] Date range picker filters correctly
- [ ] CSV export downloads with correct content
- [ ] Config form loads current values and saves successfully
- [ ] Config POST triggers Arduino update (verify via LCD thresholds)
- [ ] Devices page lists all known devices
- [ ] All API JSON responses match documented shapes
