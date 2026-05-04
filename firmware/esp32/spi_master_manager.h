/**
 * spi_master_manager.h
 * AcouSense ESP32 Firmware — SPI Master Manager
 *
 * Polls the Arduino every 10 seconds for audio reports, sends
 * configuration packets on demand, and manages heartbeat verification.
 *
 * Transaction model:
 *   - The ESP32 always transmits (MOSI) a maximum-length frame so the
 *     Arduino has enough clock cycles to send back its full response.
 *   - For a poll: ESP32 sends [heartbeat 5 bytes + 11 padding bytes = 16 bytes]
 *     while Arduino simultaneously sends [AudioReport 16 bytes].
 *   - For a config push: ESP32 sends [config 14 bytes] while Arduino
 *     simultaneously sends [ACK 7 bytes + padding].
 *
 * Spec: docs/specs/04_esp32_firmware_spec.md §3.1
 *       docs/specs/02_spi_communication_spec.md
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

class SPIMasterManager {
public:
    /**
     * Initialize the SPI master bus and push the current config to Arduino.
     * Must be called after ESP32ConfigManager::init().
     */
    static void init();

    /**
     * Non-blocking update — call every loop() iteration.
     * Handles 10-second poll cycle and heartbeat verification.
     */
    static void update();

    /**
     * Immediately push a Config packet to the Arduino.
     * Called by ESP32ConfigManager::apply() when config changes.
     * Returns true if Arduino ACKed successfully.
     */
    static bool pushConfig(const ESPConfig& cfg);

    /** Current Arduino communication health. */
    static ArduinoStatus getStatus();

    /** Timestamp (millis) of last successful AudioReport receipt. */
    static unsigned long getLastReportTime();

    /** Last received AudioReport payload (valid only if getLastReportTime() > 0). */
    static AudioReportPayload getLastReport();

private:
    static ArduinoStatus   _status;
    static unsigned long   _lastPollTime;
    static unsigned long   _lastReportTime;
    static uint8_t         _consecutiveFailures;
    static AudioReportPayload _lastReport;

    /**
     * Perform one SPI transaction: send txBuf (txLen bytes) while receiving
     * rxBuf. Clock max(txLen, AUDIO_REPORT_PACKET_SIZE) bytes total.
     * Returns true if a valid packet was parsed from rxBuf.
     */
    static bool  _transaction(const uint8_t* txBuf, uint8_t txLen,
                               uint8_t* rxBuf, uint8_t* rxLenOut);

    static bool  _poll();         // Send heartbeat, receive audio report
    static bool  _sendAndACK(const uint8_t* packet, uint8_t len, uint8_t expectedAckType);
};
