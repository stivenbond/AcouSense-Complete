/**
 * spi_slave_manager.cpp
 * AcouSense Arduino Firmware — SPI Slave Manager
 */

#include "spi_slave_manager.h"
#include "sensor_manager.h"
#include "config_manager.h"
#include <SPI.h>
#include <Arduino.h>
#include <string.h>

#define SS_PIN              10
#define HEARTBEAT_TIMEOUT   35000UL  // ms — mark offline after 35s of no heartbeat

// ─── Volatile ISR-shared state ───────────────────────────────────────────────

volatile uint8_t  SPISlaveManager::_isrTxBuf[MAX_PACKET_SIZE] = {0};
volatile uint8_t  SPISlaveManager::_isrTxLen   = 0;
volatile uint8_t  SPISlaveManager::_isrTxIdx   = 0;
volatile uint8_t  SPISlaveManager::_isrRxBuf[MAX_PACKET_SIZE] = {0};
volatile uint8_t  SPISlaveManager::_isrRxIdx   = 0;
volatile bool     SPISlaveManager::_isrRxComplete = false;

// ─── Main-loop state ─────────────────────────────────────────────────────────

bool          SPISlaveManager::_ssWasLow        = false;
uint8_t       SPISlaveManager::_rxBuf[MAX_PACKET_SIZE] = {0};
uint8_t       SPISlaveManager::_rxIdx           = 0;
bool          SPISlaveManager::_rxComplete       = false;
uint8_t       SPISlaveManager::_txBuf[MAX_PACKET_SIZE] = {0};
uint8_t       SPISlaveManager::_txLen           = 0;
unsigned long SPISlaveManager::_lastHeartbeatTime = 0;

// ─── SPI Transfer Complete ISR ───────────────────────────────────────────────

ISR(SPI_STC_vect) {
    uint8_t received = SPDR;

    // Send next TX byte (or 0x00 if nothing staged)
    if (SPISlaveManager::_isrTxIdx < SPISlaveManager::_isrTxLen) {
        SPDR = SPISlaveManager::_isrTxBuf[SPISlaveManager::_isrTxIdx++];
    } else {
        SPDR = 0x00;
    }

    // Store received byte
    if (!SPISlaveManager::_isrRxComplete &&
        SPISlaveManager::_isrRxIdx < MAX_PACKET_SIZE)
    {
        SPISlaveManager::_isrRxBuf[SPISlaveManager::_isrRxIdx++] = received;

        // Detect complete packet: need at least PACKET_OVERHEAD bytes
        // and rxIdx == PACKET_OVERHEAD + payloadLen
        if (SPISlaveManager::_isrRxIdx >= PACKET_OVERHEAD) {
            uint8_t payloadLen = SPISlaveManager::_isrRxBuf[2];
            uint8_t expectedTotal = PACKET_OVERHEAD + payloadLen;
            if (SPISlaveManager::_isrRxIdx >= expectedTotal) {
                SPISlaveManager::_isrRxComplete = true;
            }
        }
    }
}

// ─── Private helpers ─────────────────────────────────────────────────────────

void SPISlaveManager::_loadTX(const uint8_t* data, uint8_t len) {
    // Disable SPI interrupt while updating shared TX buffer
    uint8_t savedSPCR = SPCR;
    SPCR &= ~_BV(SPIE);

    uint8_t safeLen = (len > MAX_PACKET_SIZE) ? MAX_PACKET_SIZE : len;
    for (uint8_t i = 0; i < safeLen; i++) {
        _isrTxBuf[i] = data[i];
    }
    _isrTxLen = safeLen;
    _isrTxIdx = 0;

    // Pre-load first byte into SPDR so it's ready when CS fires
    SPDR = (safeLen > 0) ? _isrTxBuf[_isrTxIdx++] : 0x00;

    SPCR = savedSPCR;  // Restore interrupt state
}

void SPISlaveManager::_stageACK(uint8_t ackType, uint8_t status) {
    AckPayload ack = { ackType, status };
    uint8_t packet[ACK_PACKET_SIZE];
    uint8_t len = buildPacket(PACKET_TYPE_ACK,
                              (const uint8_t*)&ack,
                              sizeof(AckPayload),
                              packet);
    _loadTX(packet, len);
}

void SPISlaveManager::_stageReport() {
    if (!SensorManager::isReportReady()) {
        // Nothing ready — stage a heartbeat ACK as placeholder
        _stageACK(PACKET_TYPE_HEARTBEAT, ACK_STATUS_OK);
        return;
    }
    AudioReportPayload report = SensorManager::getReport();
    SensorManager::clearReport();

    uint8_t packet[AUDIO_REPORT_PACKET_SIZE];
    uint8_t len = buildPacket(PACKET_TYPE_AUDIO_REPORT,
                              (const uint8_t*)&report,
                              sizeof(AudioReportPayload),
                              packet);
    _loadTX(packet, len);
}

void SPISlaveManager::_processPacket() {
    // Copy from volatile ISR buffer (interrupts already disabled momentarily)
    uint8_t buf[MAX_PACKET_SIZE];
    uint8_t rxLen;

    SPCR &= ~_BV(SPIE);
    rxLen = _isrRxIdx;
    for (uint8_t i = 0; i < rxLen; i++) buf[i] = _isrRxBuf[i];
    SPCR |= _BV(SPIE);

    // Validate packet
    ParseResult result = parsePacket(buf, rxLen);

    if (result != PARSE_OK) {
        uint8_t status = (result == PARSE_CRC_FAIL)  ? ACK_STATUS_CRC_FAIL :
                         (result == PARSE_BUFFER_SHORT || result == PARSE_BAD_START ||
                          result == PARSE_BAD_END)    ? ACK_STATUS_CRC_FAIL :
                                                        ACK_STATUS_UNKNOWN_TYPE;
        _stageACK(buf[1], status);
        return;
    }

    uint8_t ptype = buf[1];

    switch (ptype) {
        case PACKET_TYPE_HEARTBEAT: {
            // Update heartbeat timestamp; stage ACK + queue report if ready
            _lastHeartbeatTime = millis();
            LCDManager::setConnectionState(ConnectionState::CONNECTED);
            _stageReport();  // Will stage report if ready, ACK otherwise
            break;
        }

        case PACKET_TYPE_CONFIG: {
            // Apply configuration
            ConfigPayload* cp = (ConfigPayload*)(buf + 3);
            Config cfg;
            cfg.low_threshold    = cp->low_threshold;
            cfg.medium_threshold = cp->medium_threshold;
            cfg.high_threshold   = cp->high_threshold;
            cfg.pattern_low      = cp->pattern_low;
            cfg.pattern_medium   = cp->pattern_medium;
            cfg.pattern_high     = cp->pattern_high;
            ConfigManager::apply(cfg);
            _stageACK(PACKET_TYPE_CONFIG, ACK_STATUS_OK);
            break;
        }

        case PACKET_TYPE_ACK:
            // ESP32 acknowledging our report — nothing to do
            _stageACK(PACKET_TYPE_ACK, ACK_STATUS_OK);
            break;

        default:
            _stageACK(ptype, ACK_STATUS_UNKNOWN_TYPE);
            break;
    }
}

// ─── Public API ──────────────────────────────────────────────────────────────

void SPISlaveManager::init() {
    pinMode(SS_PIN, INPUT);   // SS is driven by master
    pinMode(MISO, OUTPUT);    // Only MISO is output in slave mode

    // Enable SPI in slave mode with interrupt
    SPCR = _BV(SPE)   // SPI enable
         | _BV(SPIE); // SPI interrupt enable
         // MSTR bit NOT set → slave mode
         // Default: MSB first, Mode 0 (CPOL=0, CPHA=0)

    // Pre-stage a heartbeat ACK so the first transaction has something to send
    _stageACK(PACKET_TYPE_HEARTBEAT, ACK_STATUS_OK);
    _lastHeartbeatTime = millis();
}

void SPISlaveManager::update() {
    unsigned long now = millis();
    bool ssNow = (digitalRead(SS_PIN) == LOW);

    // Detect SS rising edge (transaction end)
    if (_ssWasLow && !ssNow) {
        // Transaction just ended — check if ISR captured a complete packet
        bool complete;
        SPCR &= ~_BV(SPIE);
        complete = _isrRxComplete;
        SPCR |= _BV(SPIE);

        if (complete) {
            _processPacket();
        }

        // Reset ISR RX state for next transaction
        SPCR &= ~_BV(SPIE);
        _isrRxIdx     = 0;
        _isrRxComplete = false;
        SPCR |= _BV(SPIE);
    }

    _ssWasLow = ssNow;

    // Heartbeat timeout → mark ESP32 offline on LCD
    if (now - _lastHeartbeatTime > HEARTBEAT_TIMEOUT) {
        LCDManager::setConnectionState(ConnectionState::OFFLINE);
    }
}
