/**
 * uart_manager.cpp
 * AcouSense ESP32 Firmware — UART Manager
 */

#include "uart_manager.h"
#include "database_manager.h"
#include "sync_manager.h"
#include <HardwareSerial.h>
#include <Arduino.h>
#include <string.h>
#include <time.h>

// UART configuration
#define ARD_UART_RX     23
#define ARD_UART_TX     19
#define UART_BAUD       115200

#define POLL_INTERVAL   10000UL                // 10 seconds
#define ACK_TIMEOUT_MS    1000UL
#define MAX_RETRIES           3
#define UNRESPONSIVE_THRESH   3

// We'll use Serial1 for Arduino communication
static HardwareSerial ArduSerial(1);

// ─── Static member definitions ───────────────────────────────────────────────

ArduinoStatus     UARTManager::_status             = ArduinoStatus::UNKNOWN;
unsigned long     UARTManager::_lastPollTime        = 0;
unsigned long     UARTManager::_lastReportTime      = 0;
uint8_t           UARTManager::_consecutiveFailures = 0;
AudioReportPayload UARTManager::_lastReport         = {};

// ─── Private helpers ─────────────────────────────────────────────────────────

bool UARTManager::_waitForPacket(uint8_t* buf, uint8_t& lenOut, unsigned long timeoutMs) {
    unsigned long start = millis();
    uint8_t idx = 0;
    
    while (millis() - start < timeoutMs) {
        while (ArduSerial.available() > 0) {
            uint8_t b = ArduSerial.read();
            
            if (idx == 0 && b != PACKET_START_BYTE) continue;
            
            buf[idx++] = b;
            
            if (idx >= 3) {
                uint8_t payloadLen = buf[2];
                uint8_t totalLen = PACKET_OVERHEAD + payloadLen;
                if (idx >= totalLen) {
                    lenOut = idx;
                    return (parsePacket(buf, idx) == PARSE_OK);
                }
            }
            
            if (idx >= MAX_PACKET_SIZE) idx = 0; // Overflow
        }
        yield();
    }
    return false;
}

bool UARTManager::_poll() {
    // Build heartbeat packet
    uint8_t txPkt[HEARTBEAT_PACKET_SIZE];
    buildPacket(PACKET_TYPE_HEARTBEAT, nullptr, 0, txPkt);

    // Clear buffer before sending
    while(ArduSerial.available()) ArduSerial.read();
    
    ArduSerial.write(txPkt, HEARTBEAT_PACKET_SIZE);
    ArduSerial.flush();

    uint8_t rxBuf[MAX_PACKET_SIZE] = {0};
    uint8_t rxLen = 0;
    
    if (!_waitForPacket(rxBuf, rxLen, ACK_TIMEOUT_MS)) {
        Serial.println(F("[UART] Poll timeout or parse error"));
        return false;
    }

    uint8_t ptype = rxBuf[1];

    if (ptype == PACKET_TYPE_AUDIO_REPORT) {
        AudioReportPayload* rpt = (AudioReportPayload*)(rxBuf + 3);
        _lastReport     = *rpt;
        _lastReportTime = millis();

        // Store to DB
        time_t now = time(nullptr);
        if (now < 1000000) now = (time_t)(millis() / 1000UL);
        DatabaseManager::insertReading(_lastReport, (int64_t)now);

        Serial.printf("[UART] Report: min=%d max=%d avg=%d alert=%d\n",
                      rpt->min_level, rpt->max_level, rpt->avg_level, rpt->alert_level);
        return true;
    }

    if (ptype == PACKET_TYPE_ACK) {
        return true;
    }

    return false;
}

bool UARTManager::_sendAndACK(const uint8_t* packet, uint8_t len,
                                    uint8_t expectedAckType) {
    for (uint8_t attempt = 0; attempt < MAX_RETRIES; attempt++) {
        while(ArduSerial.available()) ArduSerial.read(); // Clear
        
        ArduSerial.write(packet, len);
        ArduSerial.flush();

        uint8_t rxBuf[MAX_PACKET_SIZE] = {0};
        uint8_t rxLen = 0;
        
        if (_waitForPacket(rxBuf, rxLen, ACK_TIMEOUT_MS)) {
            if (rxBuf[1] == PACKET_TYPE_ACK) {
                AckPayload* ack = (AckPayload*)(rxBuf + 3);
                if (ack->acknowledged_type == expectedAckType &&
                    ack->status == ACK_STATUS_OK) {
                    return true;
                }
            }
        }
        delay(50);
    }
    return false;
}

// ─── Public API ──────────────────────────────────────────────────────────────

void UARTManager::init() {
    ArduSerial.begin(UART_BAUD, SERIAL_8N1, ARD_UART_RX, ARD_UART_TX);
    Serial.println(F("[UART] Master initialized"));

    const ESPConfig& cfg = ESP32ConfigManager::get();
    pushConfig(cfg);

    _lastPollTime = millis();
}

void UARTManager::update() {
    unsigned long now = millis();
    if (now - _lastPollTime < POLL_INTERVAL) return;
    _lastPollTime = now;

    bool ok = _poll();

    if (ok) {
        _consecutiveFailures = 0;
        _status = ArduinoStatus::ONLINE;
    } else {
        _consecutiveFailures++;
        Serial.print(F("[UART] Poll failed. Failures: "));
        Serial.println(_consecutiveFailures);

        if (_consecutiveFailures >= UNRESPONSIVE_THRESH) {
            _status = ArduinoStatus::UNRESPONSIVE;
            Serial.println(F("[UART] Arduino marked UNRESPONSIVE"));
        }
    }
}

bool UARTManager::pushConfig(const ESPConfig& cfg) {
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
        Serial.println(F("[UART] Config pushed to Arduino"));
    } else {
        Serial.println(F("[UART] Config push failed"));
    }
    return ok;
}

ArduinoStatus    UARTManager::getStatus()        { return _status; }
unsigned long    UARTManager::getLastReportTime() { return _lastReportTime; }
AudioReportPayload UARTManager::getLastReport()   { return _lastReport; }
