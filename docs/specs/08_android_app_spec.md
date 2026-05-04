# Android Application Specification

## 1. Overview

The AcouSense Android application is a **personal exposure analysis platform**, not a device controller. Its purpose is to collect user-specific noise exposure records delivered via Bluetooth synchronization, and transform them into understandable health reports with actionable AI-generated recommendations.

The distinction from the web dashboard is fundamental:
- **Web dashboard** → manages the ESP32 device (admin tool)
- **Android app** → serves the user (personal health tool)

---

## 2. Technology Stack

| Layer | Technology |
|---|---|
| Language | Kotlin |
| UI Framework | Jetpack Compose with Material 3 Expressive |
| Architecture | MVVM + Repository pattern |
| Local Database | Room (SQLite) |
| BLE | Android BLE APIs (BluetoothLeAdvertiser, BluetoothGattServer) |
| Charts | Vico (Jetpack Compose chart library) |
| AI Inference | MediaPipe LLM Inference API (Gemma 2B) |
| Background Work | WorkManager |
| DI | Hilt |

---

## 3. App Architecture

```
UI Layer (Compose Screens)
    ↕
ViewModel Layer (StateFlow, state management)
    ↕
Repository Layer (single source of truth)
    ├── ExposureRepository     — Room DB access
    ├── BleRepository          — BLE server + sync reception
    └── AiRepository           — Gemma inference
    ↕
Data Sources
    ├── Local Room DB          — exposure_records, sync_sessions
    └── MediaPipe LLM          — on-device model inference
```

---

## 4. BLE Role (Android)

The Android app operates as a **BLE Peripheral** (GATT Server + Advertiser).

- Advertises AcouSense service UUID: `0000AC00-0000-1000-8000-00805F9B34FB`
- Hosts GATT server with characteristics `0xAC01`, `0xAC02`, `0xAC03`
- When ESP32 writes to `0xAC01`: receive binary SyncPayload, parse, store in Room DB
- When received: notify ESP32 via `0xAC03` (ACK notify)

**Background behavior**: BLE advertising and GATT server must remain active while the app is running in the background. Use a foreground service with a persistent notification.

---

## 5. Room Database Schema

### 5.1 `sync_sessions`

Stores each received 5-minute sync period from an ESP32.

```kotlin
@Entity(tableName = "sync_sessions")
data class SyncSession(
    @PrimaryKey(autoGenerate = true) val id: Long = 0,
    val deviceId: String,           // ESP32 MAC as hex string
    val tsStart: Long,              // Unix timestamp
    val tsEnd: Long,
    val avgNoise: Int,              // 0-100
    val peakNoise: Int,             // 0-100
    val exposureClass: Int,         // 0=safe .. 4=danger
    val receivedAt: Long            // local device timestamp when stored
)
```

**Indexes**: `tsStart`, `deviceId`

---

### 5.2 `daily_summaries` (computed, cached)

Pre-aggregated summaries built nightly by WorkManager to feed recommendations efficiently.

```kotlin
@Entity(tableName = "daily_summaries")
data class DailySummary(
    @PrimaryKey val dateEpochDay: Long,     // LocalDate.toEpochDay()
    val totalMinutesExposed: Int,
    val avgNoiseForDay: Float,
    val peakNoiseForDay: Int,
    val minutesAboveThreshold: Int,         // Above class 2 (moderate)
    val highRiskEventCount: Int             // Sessions with class ≥ 3
)
```

---

## 6. Screens & Navigation

```
Bottom Navigation:
├── Dashboard      (home icon)
├── History        (chart icon)
├── Recommendations (brain/AI icon)
└── Settings       (gear icon)
```

---

## 7. Screen Specifications

### 7.1 Dashboard Screen

**Purpose**: Immediate personal exposure overview.

**Displayed**:
- Today's exposure summary: total time exposed, average noise, peak noise
- Alert if today's exposure exceeds WHO recommended limits
- Current BLE status: `ADVERTISING` / `SYNCING` / `IDLE`
- Last sync timestamp and device name
- Weekly exposure mini bar chart (7-day history)

---

### 7.2 History Screen

**Purpose**: Longitudinal exposure record.

**Features**:
- Daily / Weekly / Monthly view toggle
- Line chart (Vico): avg noise over selected period
- Heatmap-style calendar: color-coded by exposure class per day
- Expandable list of sync sessions per day with timestamps

---

### 7.3 Recommendations Screen

**Purpose**: AI-generated health guidance based on exposure history.

**Trigger**: User taps "Generate Recommendations" or automatic refresh once per day (WorkManager).

**Generation Flow**:
1. Load last 7 days of `daily_summaries` from Room
2. Build structured prompt (see Section 8)
3. Run inference via MediaPipe LLM Inference API (Gemma 2B, on-device)
4. Stream response tokens to UI
5. Cache result in SharedPreferences until next day

**UI**:
- Loading skeleton with "Analyzing your exposure data…" message
- Streamed recommendation text rendered in markdown-like format
- Last generated timestamp shown
- "Regenerate" button

---

### 7.4 Settings Screen

**Features**:
- User identifier display (read-only, randomly generated UUID on first install)
- "Pair New Device" button → triggers pairing flow
- Known devices list with "Remove" action
- Notification preferences (enable/disable high-exposure alerts)
- "Clear All Data" (destructive, requires confirmation)

---

## 8. AI Recommendation Engine

### 8.1 Model

**Model**: Gemma 2B (Gemma 2 2B Instruction Tuned)  
**Runtime**: MediaPipe LLM Inference API  
**Deployment**: Bundled or downloaded on first launch (~1.5 GB model file)  
**Inference**: Fully on-device, no network request

---

### 8.2 Prompt Structure

```
System:
You are a health assistant that analyzes noise exposure data and gives practical advice based on WHO hearing health guidelines. Be concise, factual, and actionable. Do not diagnose. Limit output to 5 bullet points.

User:
Here is my noise exposure summary for the last 7 days:

- Day 1: Avg noise 65/100, Peak 88/100, 2h 15m exposed, 3 high-risk events
- Day 2: Avg noise 42/100, Peak 60/100, 45m exposed, 0 high-risk events
...
- Day 7: Avg noise 78/100, Peak 95/100, 4h exposed, 7 high-risk events

My weekly average: 62/100. Days above moderate threshold: 5/7.

What should I do to protect my hearing?
```

---

### 8.3 Output Format

The model outputs 4–6 bullet points such as:
- Reduce daily exposure duration in high-noise areas
- Use hearing protection (earmuffs or earplugs) if noise regularly exceeds your moderate threshold
- Take regular recovery breaks of 10+ minutes in quiet areas between exposure periods
- Seek a professional hearing check if high-risk events occur more than 3 times per week

---

## 9. Pairing Flow

1. User navigates to Settings → "Pair New Device"
2. App checks BLE permissions (request if not granted)
3. App shows instruction screen: "Make sure you are near the AcouSense device"
4. App connects to ESP32's pairing GATT server (ESP32 temporarily acts as Peripheral)
5. App writes `user_identifier` (stored UUID) to `0xAC02`
6. ESP32 stores the MAC + user_identifier → confirmed via response
7. App shows "Paired! Your device will now sync automatically."
8. App returns to advertising mode as GATT server

---

## 10. Background Foreground Service

```kotlin
class BleGattService : Service() {
    // Runs BLE advertisement + GATT server in foreground
    // Notification: "AcouSense is syncing your exposure data"
    // Started on app launch, restarts on boot via BOOT_COMPLETED receiver
}
```

---

## 11. Permissions Required

```xml
<uses-permission android:name="android.permission.BLUETOOTH_ADVERTISE" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
<uses-permission android:name="android.permission.BLUETOOTH_SCAN" />
<uses-permission android:name="android.permission.FOREGROUND_SERVICE" />
<uses-permission android:name="android.permission.FOREGROUND_SERVICE_CONNECTED_DEVICE" />
<uses-permission android:name="android.permission.RECEIVE_BOOT_COMPLETED" />
```

---

## 12. File Structure

```
android/
├── app/
│   ├── src/main/
│   │   ├── java/com/acousense/
│   │   │   ├── MainActivity.kt
│   │   │   ├── ui/
│   │   │   │   ├── dashboard/
│   │   │   │   │   ├── DashboardScreen.kt
│   │   │   │   │   └── DashboardViewModel.kt
│   │   │   │   ├── history/
│   │   │   │   │   ├── HistoryScreen.kt
│   │   │   │   │   └── HistoryViewModel.kt
│   │   │   │   ├── recommendations/
│   │   │   │   │   ├── RecommendationsScreen.kt
│   │   │   │   │   └── RecommendationsViewModel.kt
│   │   │   │   └── settings/
│   │   │   │       ├── SettingsScreen.kt
│   │   │   │       └── SettingsViewModel.kt
│   │   │   ├── data/
│   │   │   │   ├── db/
│   │   │   │   │   ├── AppDatabase.kt
│   │   │   │   │   ├── SyncSessionDao.kt
│   │   │   │   │   └── DailySummaryDao.kt
│   │   │   │   ├── ble/
│   │   │   │   │   ├── BleGattService.kt
│   │   │   │   │   ├── BleConstants.kt
│   │   │   │   │   └── SyncPayloadParser.kt
│   │   │   │   └── ai/
│   │   │   │       ├── GemmaInference.kt
│   │   │   │       └── PromptBuilder.kt
│   │   │   ├── domain/
│   │   │   │   ├── ExposureRepository.kt
│   │   │   │   ├── BleRepository.kt
│   │   │   │   └── AiRepository.kt
│   │   │   └── di/
│   │   │       └── AppModule.kt
│   │   ├── res/
│   │   └── AndroidManifest.xml
│   └── build.gradle.kts
├── build.gradle.kts
└── settings.gradle.kts
```

---

## 13. Validation Checklist

- [ ] BLE GATT server starts and is discoverable by the ESP32
- [ ] Sync payload (52 bytes) is received and parsed correctly
- [ ] Parsed session is stored in Room DB
- [ ] ACK notify is sent to ESP32 after storage
- [ ] Dashboard shows correct today's summary
- [ ] History chart renders data from Room DB
- [ ] Gemma inference runs on-device without internet and returns valid output
- [ ] Pairing flow writes user_identifier to ESP32
- [ ] Foreground service survives app background and phone restart
- [ ] All BLE permissions are requested and handled on Android 12+
