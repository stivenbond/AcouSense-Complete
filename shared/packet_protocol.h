/**
 * packet_protocol.h
 * AcouSense — Shared SPI Packet Protocol
 *
 * Single source of truth for packet framing, types, payload structures,
 * and CRC8 implementation. Copy this file into firmware/arduino/ and
 * firmware/esp32/ — do NOT reference it from parent directories.
 *
 * Spec: docs/specs/02_spi_communication_spec.md
 */

#pragma once
#include <stdint.h>

// ─── Framing Constants ──────────────────────────────────────────────────────

#define PACKET_START_BYTE   0xAA
#define PACKET_END_BYTE     0x55

// ─── Packet Type IDs ────────────────────────────────────────────────────────

#define PACKET_TYPE_AUDIO_REPORT    0x01  // Arduino → ESP32
#define PACKET_TYPE_CONFIG          0x02  // ESP32 → Arduino
#define PACKET_TYPE_ACK             0x03  // Bidirectional
#define PACKET_TYPE_HEARTBEAT       0x04  // ESP32 → Arduino

// ─── ACK Status Codes ───────────────────────────────────────────────────────

#define ACK_STATUS_OK           0x00
#define ACK_STATUS_CRC_FAIL     0x01
#define ACK_STATUS_UNKNOWN_TYPE 0x02

// ─── Payload Structures ─────────────────────────────────────────────────────

/**
 * AudioReportPayload — sent every 10 seconds from Arduino to ESP32 (0x01)
 * Total payload size: 11 bytes
 */
typedef struct __attribute__((packed)) {
    uint32_t timestamp;     // Seconds since Arduino boot (millis()/1000)
    uint16_t min_level;     // Minimum normalized sound level (0-100)
    uint16_t max_level;     // Maximum normalized sound level (0-100)
    uint16_t avg_level;     // Average normalized sound level (0-100)
    uint8_t  alert_level;   // 0=none, 1=low, 2=medium, 3=high
} AudioReportPayload;

/**
 * ConfigPayload — sent from ESP32 to Arduino on config change (0x02)
 * Total payload size: 9 bytes
 */
typedef struct __attribute__((packed)) {
    uint16_t low_threshold;     // ADC normalized level for low alert
    uint16_t medium_threshold;  // ADC normalized level for medium alert
    uint16_t high_threshold;    // ADC normalized level for high alert
    uint8_t  pattern_low;       // Buzzer pattern ID for low severity
    uint8_t  pattern_medium;    // Buzzer pattern ID for medium severity
    uint8_t  pattern_high;      // Buzzer pattern ID for high severity
} ConfigPayload;

/**
 * AckPayload — bidirectional acknowledgment (0x03)
 * Total payload size: 2 bytes
 */
typedef struct __attribute__((packed)) {
    uint8_t acknowledged_type;  // Packet type being acknowledged
    uint8_t status;             // ACK_STATUS_* constant
} AckPayload;

// ─── Packet Size Limits ─────────────────────────────────────────────────────

#define PACKET_OVERHEAD     5   // start + type + length + crc + end
#define MAX_PAYLOAD_SIZE    255
#define MAX_PACKET_SIZE     (PACKET_OVERHEAD + MAX_PAYLOAD_SIZE)

// Audio report full packet: 5 + 11 = 16 bytes
#define AUDIO_REPORT_PACKET_SIZE  (PACKET_OVERHEAD + sizeof(AudioReportPayload))
// Config full packet:       5 + 9  = 14 bytes
#define CONFIG_PACKET_SIZE        (PACKET_OVERHEAD + sizeof(ConfigPayload))
// ACK full packet:          5 + 2  = 7 bytes
#define ACK_PACKET_SIZE           (PACKET_OVERHEAD + sizeof(AckPayload))
// Heartbeat full packet:    5 + 0  = 5 bytes
#define HEARTBEAT_PACKET_SIZE     PACKET_OVERHEAD

// ─── CRC8 (Dallas/Maxim, polynomial 0x31) ───────────────────────────────────

/**
 * Computes CRC8 over the provided data buffer.
 * Input: bytes from Packet Type through last payload byte (inclusive).
 * Start Byte and End Byte are excluded.
 */
static inline uint8_t crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0x00;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x31;
            else            crc <<= 1;
        }
    }
    return crc;
}

// ─── Packet Builder ─────────────────────────────────────────────────────────

/**
 * Builds a framed packet into outBuffer.
 * Returns total packet length in bytes.
 * outBuffer must be at least (PACKET_OVERHEAD + payloadLen) bytes.
 */
static inline uint8_t buildPacket(uint8_t type,
                                   const uint8_t *payload,
                                   uint8_t payloadLen,
                                   uint8_t *outBuffer) {
    outBuffer[0] = PACKET_START_BYTE;
    outBuffer[1] = type;
    outBuffer[2] = payloadLen;
    for (uint8_t i = 0; i < payloadLen; i++) {
        outBuffer[3 + i] = payload[i];
    }
    // CRC covers: type + payloadLen + payload
    outBuffer[3 + payloadLen] = crc8(&outBuffer[1], 2 + payloadLen);
    outBuffer[4 + payloadLen] = PACKET_END_BYTE;
    return 5 + payloadLen;
}

// ─── Parse Result ───────────────────────────────────────────────────────────

typedef enum {
    PARSE_OK            = 0,
    PARSE_BAD_START     = 1,
    PARSE_BAD_END       = 2,
    PARSE_CRC_FAIL      = 3,
    PARSE_UNKNOWN_TYPE  = 4,
    PARSE_BUFFER_SHORT  = 5,
} ParseResult;

/**
 * Validates a received packet buffer.
 * expectedLen = PACKET_OVERHEAD + payloadLen (known from packet type or buffer[2])
 * Returns PARSE_OK if packet is valid.
 */
static inline ParseResult parsePacket(const uint8_t *buffer, uint8_t bufLen) {
    if (bufLen < PACKET_OVERHEAD)          return PARSE_BUFFER_SHORT;
    if (buffer[0] != PACKET_START_BYTE)    return PARSE_BAD_START;
    uint8_t payloadLen = buffer[2];
    uint8_t totalLen   = PACKET_OVERHEAD + payloadLen;
    if (bufLen < totalLen)                 return PARSE_BUFFER_SHORT;
    if (buffer[totalLen - 1] != PACKET_END_BYTE) return PARSE_BAD_END;
    uint8_t computed = crc8(&buffer[1], 2 + payloadLen);
    if (computed != buffer[3 + payloadLen]) return PARSE_CRC_FAIL;
    return PARSE_OK;
}
