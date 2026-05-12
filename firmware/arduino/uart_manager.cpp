/**
 * uart_manager.cpp
 * AcouSense Arduino Firmware — UART Manager
 */

#include "uart_manager.h"
#include "sensor_manager.h"
#include "config_manager.h"
#include <Arduino.h>
#include <string.h>

#define HEARTBEAT_TIMEOUT   35000UL  // ms — mark offline after 35s of no heartbeat
#define UART_BAUD           115200

// ─── Main-loop state ─────────────────────────────────────────────────────────

uint8_t       UARTManager::_rxBuf[MAX_PACKET_SIZE] = {0};
uint8_t       UARTManager::_rxIdx           = 0;
unsigned long UARTManager::_lastHeartbeatTime = 0;

// ─── Private helpers ─────────────────────────────────────────────────────────

void UARTManager::_transmit(const uint8_t* data, uint8_t len) {
    Serial.write(data, len);
    Serial.flush();
}

void UARTManager::_sendACK(uint8_t ackType, uint8_t status) {
    AckPayload ack = { ackType, status };
    uint8_t packet[ACK_PACKET_SIZE];
    uint8_t len = buildPacket(PACKET_TYPE_ACK,
                              (const uint8_t*)&ack,
                              sizeof(AckPayload),
                              packet);
    _transmit(packet, len);
}

void UARTManager::_sendReport() {
    if (!SensorManager::isReportReady()) {
        // Nothing ready — send a heartbeat ACK as placeholder
        _sendACK(PACKET_TYPE_HEARTBEAT, ACK_STATUS_OK);
        return;
    }
    AudioReportPayload report = SensorManager::getReport();
    SensorManager::clearReport();

    uint8_t packet[AUDIO_REPORT_PACKET_SIZE];
    uint8_t len = buildPacket(PACKET_TYPE_AUDIO_REPORT,
                              (const uint8_t*)&report,
                              sizeof(AudioReportPayload),
                              packet);
    _transmit(packet, len);
}

void UARTManager::_processPacket() {
    // Validate packet
    ParseResult result = parsePacket(_rxBuf, _rxIdx);

    if (result != PARSE_OK) {
        // If we have a bad packet, we should probably just clear and ignore or send NACK
        // But the protocol expects an ACK for most things.
        if (_rxIdx > 1) {
             uint8_t status = (result == PARSE_CRC_FAIL)  ? ACK_STATUS_CRC_FAIL :
                             (result == PARSE_BUFFER_SHORT || result == PARSE_BAD_START ||
                              result == PARSE_BAD_END)    ? ACK_STATUS_CRC_FAIL :
                                                            ACK_STATUS_UNKNOWN_TYPE;
            _sendACK(_rxBuf[1], status);
        }
        return;
    }

    uint8_t ptype = _rxBuf[1];

    switch (ptype) {
        case PACKET_TYPE_HEARTBEAT: {
            _lastHeartbeatTime = millis();
            LCDManager::setConnectionState(ConnectionState::CONNECTED);
            _sendReport();  // Response to heartbeat: report or ACK
            break;
        }

        case PACKET_TYPE_CONFIG: {
            ConfigPayload* cp = (ConfigPayload*)(_rxBuf + 3);
            Config cfg;
            cfg.low_threshold    = cp->low_threshold;
            cfg.medium_threshold = cp->medium_threshold;
            cfg.high_threshold   = cp->high_threshold;
            cfg.pattern_low      = cp->pattern_low;
            cfg.pattern_medium   = cp->pattern_medium;
            cfg.pattern_high     = cp->pattern_high;
            ConfigManager::apply(cfg);
            _sendACK(PACKET_TYPE_CONFIG, ACK_STATUS_OK);
            break;
        }

        case PACKET_TYPE_ACK:
            // ESP32 acknowledging our report — nothing to do
            break;

        default:
            _sendACK(ptype, ACK_STATUS_UNKNOWN_TYPE);
            break;
    }
}

// ─── Public API ──────────────────────────────────────────────────────────────

void UARTManager::init() {
    Serial.begin(UART_BAUD);
    _lastHeartbeatTime = millis();
    // In UART mode, we don't pre-stage like SPI. We just wait for incoming.
}

void UARTManager::update() {
    unsigned long now = millis();

    // Read from Serial
    while (Serial.available() > 0) {
        uint8_t b = Serial.read();
        
        // Basic framing state machine
        if (_rxIdx == 0 && b != PACKET_START_BYTE) {
            continue; // Skip until start byte
        }

        _rxBuf[_rxIdx++] = b;

        // Check if we have enough to determine length
        if (_rxIdx >= 3) {
            uint8_t payloadLen = _rxBuf[2];
            uint8_t expectedTotal = PACKET_OVERHEAD + payloadLen;
            
            if (_rxIdx >= expectedTotal) {
                // Packet complete
                _processPacket();
                _rxIdx = 0; // Reset for next packet
            }
        }
        
        // Safety: don't overflow
        if (_rxIdx >= MAX_PACKET_SIZE) {
            _rxIdx = 0;
        }
    }

    // Heartbeat timeout → mark ESP32 offline on LCD
    if (now - _lastHeartbeatTime > HEARTBEAT_TIMEOUT) {
        LCDManager::setConnectionState(ConnectionState::OFFLINE);
    }
}
