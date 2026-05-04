/**
 * lcd_manager.h
 * AcouSense Arduino Firmware — LCD Manager
 *
 * Controls the 16x2 I2C LCD display. Refreshes every 500ms.
 * Displays current sound level, alert state, and ESP32 connection health.
 *
 * Spec: docs/specs/03_arduino_firmware_spec.md §3.2
 *
 * Library required: LiquidCrystal_I2C by Frank de Brabander
 */

#pragma once
#include <stdint.h>

/**
 * Connection states reported by SPISlaveManager.
 * LCDManager reads this to show ESP32 link health.
 */
enum class ConnectionState : uint8_t {
    UNKNOWN     = 0,   // Boot state
    CONNECTED   = 1,   // Heartbeat received within timeout
    OFFLINE     = 2,   // No heartbeat for > 30 seconds
};

class LCDManager {
public:
    /**
     * Initialize LCD hardware.
     * @param i2cAddress  PCF8574 I2C address (0x27 or 0x3F — use scanner test first)
     */
    static void init(uint8_t i2cAddress = 0x27);

    /**
     * Non-blocking update — call every loop() iteration.
     * Refreshes display every 500ms.
     */
    static void update();

    /** Update the connection state shown on line 2. */
    static void setConnectionState(ConnectionState state);

private:
    static uint8_t          _address;
    static unsigned long    _lastRefreshTime;
    static ConnectionState  _connState;

    static void _render(uint16_t level, uint8_t alertLevel);
};
