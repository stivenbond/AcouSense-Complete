/**
 * sync_manager.cpp
 * AcouSense ESP32 Firmware — Sync Manager
 */

#include "sync_manager.h"
#include "database_manager.h"
#include "bluetooth_manager.h"
#include "ble_constants.h"
#include "exposure_classes.h"
#include <WiFi.h>
#include <Arduino.h>
#include <time.h>
#include <string.h>

#define SYNC_CYCLE_MS  300000UL   // 5 minutes

unsigned long SyncManager::_lastCycleTime = 0;

// ─── Aggregation context ──────────────────────────────────────────────────────

struct AggResult {
    int32_t  minNoise;
    int32_t  maxNoise;
    int64_t  sumAvg;
    int      count;
};

static void aggCB(const NoiseRecord& rec, void* ud) {
    AggResult* a = (AggResult*)ud;
    if (rec.min_level < a->minNoise) a->minNoise = (int32_t)rec.min_level;
    if (rec.max_level > a->maxNoise) a->maxNoise = (int32_t)rec.max_level;
    a->sumAvg += rec.avg_level;
    a->count++;
}

// ─── Cycle implementation ─────────────────────────────────────────────────────

void SyncManager::_runCycle() {
    Serial.println(F("[Sync] Starting 5-minute sync cycle"));

    int64_t tsEnd   = (int64_t)time(nullptr);
    int64_t tsStart = tsEnd - 300;  // 5 minutes ago

    // Aggregate noise data for the window
    AggResult agg = { 100, 0, 0, 0 };
    DatabaseManager::getReadings(tsStart, tsEnd, 0, 0, aggCB, &agg);

    if (agg.count == 0) {
        Serial.println(F("[Sync] No readings in window — skipping"));
        BluetoothManager::resetPresence();
        return;
    }

    uint16_t avgNoise  = (uint16_t)(agg.sumAvg / agg.count);
    uint16_t peakNoise = (uint16_t)agg.maxNoise;
    uint8_t  expClass  = classifyExposure(avgNoise);

    Serial.printf("[Sync] Window: avg=%d peak=%d class=%s\n",
                  avgNoise, peakNoise, EXPOSURE_CLASS_LABELS[expClass]);

    // Get ESP32 MAC address for device_id field
    uint8_t espMac[6];
    esp_read_mac(espMac, ESP_MAC_BT);

    // Deliver to all present devices
    int presenceCount;
    PresenceEntry* table = BluetoothManager::getPresenceTable(presenceCount);

    int delivered = 0;
    for (int i = 0; i < presenceCount; i++) {
        if (!table[i].present) continue;

        SyncPayload payload = {};
        memcpy(payload.device_id, espMac, 6);
        strncpy((char*)payload.user_id, table[i].userId, 31);
        payload.ts_start       = (uint32_t)tsStart;
        payload.ts_end         = (uint32_t)tsEnd;
        payload.avg_noise      = avgNoise;
        payload.peak_noise     = peakNoise;
        payload.exposure_class = expClass;

        bool ok = BluetoothManager::deliverSync(table[i].mac, payload);
        if (ok) {
            delivered++;
            Serial.printf("[Sync] Delivered to %s\n", table[i].mac);
        } else {
            Serial.printf("[Sync] Failed delivery to %s\n", table[i].mac);
        }
    }

    Serial.printf("[Sync] Cycle complete: %d/%d delivered\n", delivered, presenceCount);

    // Record sync timestamp and reset presence
    BluetoothManager::setLastSyncTs(tsEnd);
    BluetoothManager::resetPresence();
}

// ─── Public API ──────────────────────────────────────────────────────────────

void SyncManager::init() {
    _lastCycleTime = millis();
}

void SyncManager::update() {
    unsigned long now = millis();
    if (now - _lastCycleTime < SYNC_CYCLE_MS) return;
    _lastCycleTime = now;
    _runCycle();
}
