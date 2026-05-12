/**
 * uart_manager.h
 * AcouSense Arduino Firmware — UART Manager
 *
 * Handles all UART communication with the ESP32.
 * - Processes complete packets in update() (main loop context).
 * - Responses are sent immediately after processing a valid packet.
 *
 * Spec: docs/specs/02_spi_communication_spec.md (Adapted for UART)
 *       docs/specs/03_arduino_firmware_spec.md §3.4
 */

#pragma once
#include <stdint.h>
#include "packet_protocol.h"
#include "lcd_manager.h"

class UARTManager {
public:
    /** Initialize UART hardware. Call once in setup(). */
    static void init();

    /**
     * Non-blocking update — call every loop() iteration.
     * Reads from Serial, parses packets, and handles heartbeat timeout.
     */
    static void update();

private:
    static uint8_t  _rxBuf[MAX_PACKET_SIZE];
    static uint8_t  _rxIdx;
    static unsigned long _lastHeartbeatTime;

    static void _processPacket();
    static void _sendACK(uint8_t ackType, uint8_t status);
    static void _sendReport();
    static void _transmit(const uint8_t* data, uint8_t len);
};
