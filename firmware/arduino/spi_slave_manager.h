/**
 * spi_slave_manager.h
 * AcouSense Arduino Firmware — SPI Slave Manager
 *
 * Handles all SPI slave communication with the ESP32 master.
 * - Uses SPI_STC_vect interrupt for byte-by-byte reception into a ring buffer.
 * - Monitors the SS (D10) pin to detect transaction boundaries.
 * - Processes complete packets in update() (main loop context).
 * - Pre-stages response bytes (ACK or AudioReport) in the TX buffer
 *   so they are ready for the next SPI transaction.
 *
 * Protocol:
 *   - The ESP32 clocks a fixed-size exchange: max(TX packet, RX packet) bytes.
 *   - The Arduino simultaneously sends whatever is pre-staged in txBuf.
 *   - After CS de-asserts, the Arduino processes the received packet
 *     and pre-stages its next response.
 *
 * Spec: docs/specs/02_spi_communication_spec.md
 *       docs/specs/03_arduino_firmware_spec.md §3.4
 */

#pragma once
#include <stdint.h>
#include "packet_protocol.h"
#include "lcd_manager.h"

class SPISlaveManager {
public:
    /** Initialize SPI slave hardware and pre-stage default TX. Call once in setup(). */
    static void init();

    /**
     * Non-blocking update — call every loop() iteration.
     * Detects SS rising edge (transaction end), processes received packet,
     * and pre-stages the next response.
     */
    static void update();

private:
    static bool     _ssWasLow;
    static uint8_t  _rxBuf[MAX_PACKET_SIZE];
    static uint8_t  _rxIdx;
    static bool     _rxComplete;
    static uint8_t  _txBuf[MAX_PACKET_SIZE];
    static uint8_t  _txLen;

    static unsigned long _lastHeartbeatTime;

    static void _processPacket();
    static void _stageACK(uint8_t ackType, uint8_t status);
    static void _stageReport();
    static void _loadTX(const uint8_t* data, uint8_t len);

public:
    // ISR-accessible state (volatile)
    static volatile uint8_t  _isrTxBuf[MAX_PACKET_SIZE];
    static volatile uint8_t  _isrTxLen;
    static volatile uint8_t  _isrTxIdx;
    static volatile uint8_t  _isrRxBuf[MAX_PACKET_SIZE];
    static volatile uint8_t  _isrRxIdx;
    static volatile bool     _isrRxComplete;
};
