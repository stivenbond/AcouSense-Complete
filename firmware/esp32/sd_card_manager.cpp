/**
 * sd_card_manager.cpp
 * AcouSense ESP32 Firmware — SD Card Manager
 */

#include "sd_card_manager.h"
#include <SPI.h>
#include <SD.h>
#include <Arduino.h>

// SD card SPI pins (separate bus from Arduino communication)
#define SD_MOSI   13
#define SD_MISO   12
#define SD_SCK    14
#define SD_CS     15

static const char DB_DIR[]  = "/acousense";
static const char DB_PATH[] = "/acousense/acousense.db";
static const char WWW_DIR[] = "/www";

SDStatus SDCardManager::_status = SDStatus::UNINITIALIZED;

// ─── Private helpers ─────────────────────────────────────────────────────────

bool SDCardManager::_ensureDirectories() {
    if (!SD.exists(DB_DIR)) {
        if (!SD.mkdir(DB_DIR)) {
            Serial.println(F("[SD] Failed to create /acousense directory"));
            return false;
        }
    }
    if (!SD.exists(WWW_DIR)) {
        SD.mkdir(WWW_DIR);  // Non-fatal — web assets may not be deployed yet
    }
    return true;
}

// ─── Public API ──────────────────────────────────────────────────────────────

void SDCardManager::init() {
    Serial.print(F("[SD] Initializing SD card... "));
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    if (!SD.begin(SD_CS)) {
        Serial.println(F("FAILED"));
        Serial.println(F("[SD] Check: card inserted, FAT32 formatted, 3.3V supply, decoupling cap"));
        _status = SDStatus::FAILED;
        return;
    }

    uint64_t cardMB = SD.cardSize() / (1024ULL * 1024ULL);
    Serial.print(F("OK ("));
    Serial.print(cardMB);
    Serial.println(F(" MB)"));

    if (!_ensureDirectories()) {
        _status = SDStatus::FAILED;
        return;
    }

    _status = SDStatus::OK;
    Serial.println(F("[SD] Directory structure verified"));
}

bool SDCardManager::isMounted() {
    return _status == SDStatus::OK;
}

SDStatus SDCardManager::getStatus() {
    return _status;
}

const char* SDCardManager::getDataDir() { return DB_DIR;  }
const char* SDCardManager::getDBPath()  { return DB_PATH; }
const char* SDCardManager::getWWWDir()  { return WWW_DIR; }

bool SDCardManager::remount() {
    if (_status == SDStatus::OK) return true;
    SD.end();
    if (SD.begin(SD_CS)) {
        _status = SDStatus::OK;
        _ensureDirectories();
        return true;
    }
    return false;
}
