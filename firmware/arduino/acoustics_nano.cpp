/**
 * acoustics_nano.cpp
 * AcouSense — Arduino Nano V3 Firmware Entry Point
 *
 * Non-blocking cooperative multitasking firmware.
 * All modules are updated every loop() iteration via millis() scheduling.
 * delay() is never used anywhere in this codebase.
 *
 * Modules:
 *   ConfigManager   — runtime threshold and pattern storage (RAM)
 *   SensorManager   — 20Hz ADC sampling, 10s report aggregation
 *   LCDManager      — 500ms LCD refresh (level, alert, connection state)
 *   BuzzerManager   — pattern-driven PWM buzzer (millis state machine)
 *   SPISlaveManager — interrupt-driven SPI slave with SS-edge packet processing
 *
 * Hardware:
 *   Microphone AO → A0
 *   LCD SDA → A4, SCL → A5 (I2C, address configurable below)
 *   Buzzer + → D9 (PWM)
 *   SPI SS → D10, MOSI → D11, MISO → D12, SCK → D13
 *   (All SPI lines pass through logic level shifter to ESP32)
 *
 * Spec: docs/specs/03_arduino_firmware_spec.md
 */

#include "config_manager.h"
#include "sensor_manager.h"
#include "lcd_manager.h"
#include "buzzer_manager.h"
#include "spi_slave_manager.h"
#include <Arduino.h>

// ─── LCD I2C Address ─────────────────────────────────────────────────────────
// Run firmware/tests/test_lcd.cpp to discover the correct address.
// Common values: 0x27 (most modules) or 0x3F (some clones)
#define LCD_I2C_ADDRESS  0x27

void setup() {
    // Initialization order matters:
    // 1. Config first — other modules read it on init
    ConfigManager::init();

    // 2. Sensor — sets up ADC pin and timing baseline
    SensorManager::init();

    // 3. LCD — uses Wire (I2C); should init after any I2C-dependent inits
    LCDManager::init(LCD_I2C_ADDRESS);

    // 4. Buzzer — configures PWM pin
    BuzzerManager::init();

    // 5. SPI slave last — enables SPI interrupt; must be after all other inits
    SPISlaveManager::init();

    // Note: No Serial.begin() in production firmware.
    // Uncomment only for debugging — UART conflicts with some SPI timing.
    // Serial.begin(115200);
}

void loop() {
    // All updates are non-blocking — call unconditionally every iteration.
    // Each module internally checks millis() for its own scheduling.

    SensorManager::update();    // ADC sample + 10s report finalization
    LCDManager::update();       // 500ms display refresh
    BuzzerManager::update();    // Pattern state machine tick
    SPISlaveManager::update();  // SS edge detection + packet processing
}
