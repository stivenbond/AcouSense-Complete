/**
 * spi_master_manager.cpp
 * AcouSense ESP32 Firmware — SPI Master Manager
 */

#include "spi_master_manager.h"
#include "database_manager.h"
#include "sync_manager.h"
#include <SPI.h>
#include <Arduino.h>
#include <string.h>
#include <time.h>

// Arduino SPI bus pins
#define ARD_SCLK   18
#define ARD_MISO   19
#define ARD_MOSI   23
#define ARD_CS      5

#define SPI_FREQ        500000UL               // 500 kHz
#define POLL_INTERVAL   10000UL                // 10 seconds
#define ACK_TIMEOUT_MS    500UL
#define MAX_RETRIES           3
#define UNRESPONSIVE_THRESH   3

// ─── Static member definitions ───────────────────────────────────────────────

ArduinoStatus     SPIMasterManager::_status             = ArduinoStatus::UNKNOWN;
unsigned long     SPIMasterManager::_lastPollTime        = 0;
unsigned long     SPIMasterManager::_lastReportTime      = 0;
uint8_t           SPIMasterManager::_consecutiveFailures = 0;
AudioReportPayload SPIMasterManager::_lastReport         = {};

// ─── Private helpers ─────────────────────────────────────────────────────────

bool SPIMasterManager::_transaction(const uint8_t* txBuf, uint8_t txLen,
                                    uint8_t* rxBuf, uint8_t* rxLenOut) {
    // Clock enough bytes for the longest expected response (AudioReport = 16 bytes)
    uint8_t totalBytes = txLen > AUDIO_REPORT_PACKET_SIZE
                          ? txLen
                          : AUDIO_REPORT_PACKET_SIZE;

    SPI.beginTransaction(SPISettings(SPI_FREQ, MSBFIRST, SPI_MODE0));
    digitalWrite(ARD_CS, LOW);
    delayMicroseconds(10);  // Brief CS setup time for Arduino to prepare

    for (uint8_t i = 0; i < totalBytes; i++) {
        uint8_t txByte = (i < txLen) ? txBuf[i] : 0x00;
        rxBuf[i] = SPI.transfer(txByte);
    }

    delayMicroseconds(10);
    digitalWrite(ARD_CS, HIGH);
    SPI.endTransaction();

    *rxLenOut = totalBytes;
    return true;
}

bool SPIMasterManager::_poll() {
    // Build heartbeat packet (5 bytes)
    uint8_t txPkt[HEARTBEAT_PACKET_SIZE];
    buildPacket(PACKET_TYPE_HEARTBEAT, nullptr, 0, txPkt);

    uint8_t rxBuf[AUDIO_REPORT_PACKET_SIZE + 4] = {0};
    uint8_t rxLen = 0;
    _transaction(txPkt, HEARTBEAT_PACKET_SIZE, rxBuf, &rxLen);

    // Parse response — looking for an AudioReport or ACK
    ParseResult pr = parsePacket(rxBuf, rxLen);
    if (pr != PARSE_OK) {
        Serial.print(F("[SPI] Poll parse error: ")); Serial.println((int)pr);
        return false;
    }

    uint8_t ptype = rxBuf[1];

    if (ptype == PACKET_TYPE_AUDIO_REPORT) {
        AudioReportPayload* rpt = (AudioReportPayload*)(rxBuf + 3);
        _lastReport     = *rpt;
        _lastReportTime = millis();

        // Store to DB
        time_t now = time(nullptr);
        if (now < 1000000) now = (time_t)(millis() / 1000UL);  // fallback if NTP not set
        DatabaseManager::insertReading(_lastReport, (int64_t)now);

        Serial.printf("[SPI] Report: min=%d max=%d avg=%d alert=%d\n",
                      rpt->min_level, rpt->max_level, rpt->avg_level, rpt->alert_level);
        return true;
    }

    if (ptype == PACKET_TYPE_ACK) {
        // Arduino had nothing to report — heartbeat acknowledged
        return true;
    }

    return false;
}

bool SPIMasterManager::_sendAndACK(const uint8_t* packet, uint8_t len,
                                    uint8_t expectedAckType) {
    for (uint8_t attempt = 0; attempt < MAX_RETRIES; attempt++) {
        uint8_t rxBuf[ACK_PACKET_SIZE + 4] = {0};
        uint8_t rxLen = 0;
        _transaction(packet, len, rxBuf, &rxLen);

        ParseResult pr = parsePacket(rxBuf, rxLen);
        if (pr == PARSE_OK && rxBuf[1] == PACKET_TYPE_ACK) {
            AckPayload* ack = (AckPayload*)(rxBuf + 3);
            if (ack->acknowledged_type == expectedAckType &&
                ack->status == ACK_STATUS_OK) {
                return true;
            }
        }
        delay(10);  // Brief inter-retry pause (acceptable — not in tight loop)
    }
    return false;
}

// ─── Public API ──────────────────────────────────────────────────────────────

void SPIMasterManager::init() {
    pinMode(ARD_CS, OUTPUT);
    digitalWrite(ARD_CS, HIGH);
    SPI.begin(ARD_SCLK, ARD_MISO, ARD_MOSI, ARD_CS);

    Serial.println(F("[SPI] Master initialized"));

    // Push current config to Arduino on boot
    const ESPConfig& cfg = ESP32ConfigManager::get();
    pushConfig(cfg);

    _lastPollTime = millis();
}

void SPIMasterManager::update() {
    unsigned long now = millis();
    if (now - _lastPollTime < POLL_INTERVAL) return;
    _lastPollTime = now;

    bool ok = _poll();

    if (ok) {
        _consecutiveFailures = 0;
        _status = ArduinoStatus::ONLINE;
    } else {
        _consecutiveFailures++;
        Serial.print(F("[SPI] Poll failed. Failures: "));
        Serial.println(_consecutiveFailures);

        if (_consecutiveFailures >= UNRESPONSIVE_THRESH) {
            _status = ArduinoStatus::UNRESPONSIVE;
            Serial.println(F("[SPI] Arduino marked UNRESPONSIVE"));
        }
    }
}

bool SPIMasterManager::pushConfig(const ESPConfig& cfg) {
    ConfigPayload cp;
    cp.low_threshold    = cfg.low_threshold;
    cp.medium_threshold = cfg.medium_threshold;
    cp.high_threshold   = cfg.high_threshold;
    cp.pattern_low      = cfg.pattern_low;
    cp.pattern_medium   = cfg.pattern_medium;
    cp.pattern_high     = cfg.pattern_high;

    uint8_t packet[CONFIG_PACKET_SIZE];
    buildPacket(PACKET_TYPE_CONFIG, (const uint8_t*)&cp, sizeof(cp), packet);

    bool ok = _sendAndACK(packet, CONFIG_PACKET_SIZE, PACKET_TYPE_CONFIG);
    if (ok) {
        Serial.println(F("[SPI] Config pushed to Arduino"));
    } else {
        Serial.println(F("[SPI] Config push failed after retries"));
    }
    return ok;
}

ArduinoStatus    SPIMasterManager::getStatus()        { return _status; }
unsigned long    SPIMasterManager::getLastReportTime() { return _lastReportTime; }
AudioReportPayload SPIMasterManager::getLastReport()   { return _lastReport; }
