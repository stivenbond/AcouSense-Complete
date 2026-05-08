/**
 * bluetooth_manager.cpp
 * AcouSense ESP32 Firmware — Bluetooth Manager
 *
 * Uses the ESP32 Arduino BLE library (BLEDevice, BLEScan, BLEClient).
 * Included with the ESP32 Arduino core — no additional installation needed.
 */

#include "bluetooth_manager.h"
#include "database_manager.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <BLERemoteService.h>
#include <BLERemoteCharacteristic.h>
#include <Arduino.h>
#include <time.h>
#include <string.h>

#define SCAN_WINDOW_SEC   5    // BLE scan window per burst
#define CONNECT_TIMEOUT_MS  10000UL
#define WRITE_TIMEOUT_MS     5000UL

// ─── Static member definitions ───────────────────────────────────────────────

BTManagerStatus BluetoothManager::_status        = BTManagerStatus::IDLE;
int64_t         BluetoothManager::_lastSyncTs    = 0;
PresenceEntry   BluetoothManager::_presence[32]  = {};
int             BluetoothManager::_presenceCount = 0;

// ─── BLE Scan Callback ───────────────────────────────────────────────────────

class AcouScanCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        // Only interested in devices advertising our AcouSense service UUID
        if (!advertisedDevice.haveServiceUUID()) return;
        if (!advertisedDevice.isAdvertisingService(
                BLEUUID(ACOUSENSE_SERVICE_UUID))) return;

        String mac = advertisedDevice.getAddress().toString().c_str();

        // Look up in DB
        BTDevice dev;
        if (!DatabaseManager::findDevice(mac.c_str(), dev)) return;  // Unknown

        // Update presence table
        int64_t now = (int64_t)time(nullptr);
        DatabaseManager::upsertDevice(mac.c_str(), dev.user_identifier, now);

        // Find or add to presence table
        for (int i = 0; i < BluetoothManager::_presenceCount; i++) {
            if (strcmp(BluetoothManager::_presence[i].mac, mac.c_str()) == 0) {
                BluetoothManager::_presence[i].present = true;
                BluetoothManager::_presence[i].ts_last_seen = now;
                return;
            }
        }
        // New entry
        if (BluetoothManager::_presenceCount < 32) {
            PresenceEntry& e = BluetoothManager::_presence[BluetoothManager::_presenceCount++];
            strncpy(e.mac, mac.c_str(), 17);
            strncpy(e.userId, dev.user_identifier, 63);
            e.mac[17] = 0;
            e.userId[63] = 0;
            e.present = true;
            e.ts_first_seen = now;
            e.ts_last_seen  = now;
        }
    }
};

static AcouScanCallbacks scanCallbacks;
static BLEScan* pScan = nullptr;
static bool scanActive = false;

static void scanCompleteCB(BLEScanResults) {
    scanActive = false;
    if (pScan) pScan->clearResults();
}

static void releaseClient(BLEClient* client) {
    if (!client) return;
    if (client->isConnected()) client->disconnect();
    delete client;
}

// ─── Private helpers ─────────────────────────────────────────────────────────

void BluetoothManager::_startScan() {
    if (!pScan) return;
    pScan->clearResults();
    scanActive = pScan->start(SCAN_WINDOW_SEC, scanCompleteCB, false);
    _status = BTManagerStatus::SCANNING;
}

// ─── Public API ──────────────────────────────────────────────────────────────

void BluetoothManager::init() {
    BLEDevice::init("AcouSense-ESP32");

    pScan = BLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(&scanCallbacks, false);
    pScan->setActiveScan(true);
    pScan->setInterval(100);
    pScan->setWindow(99);

    _startScan();
    Serial.println(F("[BT] BLE scanning started"));
}

void BluetoothManager::update() {
    // Restart scan when the async scan-complete callback marks it finished.
    if (_status == BTManagerStatus::SCANNING && !scanActive) {
        _startScan();  // Continuous bursts
    }
}

PresenceEntry* BluetoothManager::getPresenceTable(int& countOut) {
    countOut = _presenceCount;
    return _presence;
}

void BluetoothManager::resetPresence() {
    for (int i = 0; i < _presenceCount; i++) {
        _presence[i].present     = false;
        _presence[i].ts_first_seen = 0;
        _presence[i].ts_last_seen  = 0;
    }
    _presenceCount = 0;
}

bool BluetoothManager::deliverSync(const char* mac, const SyncPayload& payload) {
    _status = BTManagerStatus::SYNCING;

    BLEClient* client = BLEDevice::createClient();
    BLEAddress addr(mac);

    Serial.print(F("[BT] Connecting to ")); Serial.println(mac);

    unsigned long connectStart = millis();
    bool connected = false;

    // Attempt connection with timeout
    while (millis() - connectStart < CONNECT_TIMEOUT_MS) {
        if (client->connect(addr)) { connected = true; break; }
        delay(100);
    }

    if (!connected) {
        Serial.println(F("[BT] Connection failed"));
        releaseClient(client);
        _status = BTManagerStatus::SCANNING;
        return false;
    }

    // Get the AcouSense service
    BLERemoteService* svc = client->getService(BLEUUID(ACOUSENSE_SERVICE_UUID));
    if (!svc) {
        Serial.println(F("[BT] Service not found on remote device"));
        releaseClient(client);
        _status = BTManagerStatus::SCANNING;
        return false;
    }

    // Get the sync payload characteristic
    BLERemoteCharacteristic* syncChar =
        svc->getCharacteristic(BLEUUID(ACOUSENSE_CHAR_SYNC_PAYLOAD));
    if (!syncChar) {
        Serial.println(F("[BT] Sync characteristic not found"));
        releaseClient(client);
        _status = BTManagerStatus::SCANNING;
        return false;
    }

    // Write the sync payload (52 bytes, binary)
    syncChar->writeValue((uint8_t*)&payload, sizeof(SyncPayload), false);
    Serial.printf("[BT] Sync payload delivered to %s\n", mac);

    delay(200);  // Brief pause — allow Android to process before disconnect
    releaseClient(client);

    _status = BTManagerStatus::SCANNING;
    return true;
}

const char* BluetoothManager::getStatusString() {
    switch (_status) {
        case BTManagerStatus::SCANNING: return "scanning";
        case BTManagerStatus::SYNCING:  return "syncing";
        case BTManagerStatus::PAIRING:  return "pairing";
        default:                        return "idle";
    }
}

int64_t BluetoothManager::getLastSyncTs()         { return _lastSyncTs; }
void    BluetoothManager::setLastSyncTs(int64_t t) { _lastSyncTs = t; }
