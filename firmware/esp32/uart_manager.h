/**
 * uart_manager.h
 * AcouSense ESP32 Firmware — UART Manager
 *
 * Polls the Arduino every 10 seconds for audio reports, sends
 * configuration packets on demand, and manages heartbeat verification.
 * Uses UART instead of SPI.
 *
 * Spec: docs/specs/04_esp32_firmware_spec.md §3.1 (Adapted for UART)
 */

#pragma once
#include <stdint.h>
#include "packet_protocol.h"
#include "config_manager.h"

enum class ArduinoStatus : uint8_t {
    UNKNOWN      = 0,
    ONLINE       = 1,
    UNRESPONSIVE = 2,
};

class UARTManager {
public:
    /**
     * Initialize the UART bus and push the current config to Arduino.
     */
    static void init();

    /**
     * Non-blocking update — call every loop() iteration.
     * Handles 10-second poll cycle and heartbeat verification.
     */
    static void update();

    /**
     * Immediately push a Config packet to the Arduino.
     * Returns true if Arduino ACKed successfully.
     */
    static bool pushConfig(const ESPConfig& cfg);

    /** Current Arduino communication health. */
    static ArduinoStatus getStatus();

    /** Timestamp (millis) of last successful AudioReport receipt. */
    static unsigned long getLastReportTime();

    /** Last received AudioReport payload. */
    static AudioReportPayload getLastReport();

private:
    static ArduinoStatus   _status;
    static unsigned long   _lastPollTime;
    static unsigned long   _lastReportTime;
    static uint8_t         _consecutiveFailures;
    static AudioReportPayload _lastReport;

    static bool  _poll();         // Send heartbeat, receive audio report
    static bool  _sendAndACK(const uint8_t* packet, uint8_t len, uint8_t expectedAckType);
    static bool  _waitForPacket(uint8_t* buf, uint8_t& len, unsigned long timeoutMs);
};
