/**
 * spi_constants.h
 * AcouSense — Shared SPI Pin & Timing Constants
 *
 * Spec: docs/specs/02_spi_communication_spec.md
 *       docs/specs/01_hardware_and_schematic_spec.md
 */

#pragma once
#include <stdint.h>

// ─── ESP32 SPI Master Pin Assignments ───────────────────────────────────────
// SPI bus used for Arduino Nano communication

#define ESP32_SPI_SCLK_PIN  18
#define ESP32_SPI_MISO_PIN  19
#define ESP32_SPI_MOSI_PIN  23
#define ESP32_SPI_CS_PIN     5   // Chip Select for Arduino Nano

// ─── ESP32 SD Card SPI Pin Assignments ──────────────────────────────────────
// Separate SPI bus for SD card

#define ESP32_SD_MOSI_PIN   13
#define ESP32_SD_MISO_PIN   12
#define ESP32_SD_SCLK_PIN   14
#define ESP32_SD_CS_PIN     15

// ─── Arduino Nano SPI Slave Pin Assignments ──────────────────────────────────
// Standard Arduino hardware SPI pins

#define ARDUINO_SPI_MOSI_PIN  11  // D11
#define ARDUINO_SPI_MISO_PIN  12  // D12
#define ARDUINO_SPI_SCLK_PIN  13  // D13
#define ARDUINO_SPI_SS_PIN    10  // D10 (Slave Select)

// ─── SPI Configuration ──────────────────────────────────────────────────────

#define SPI_CLOCK_HZ        500000UL   // 500 kHz — conservative for stability
#define SPI_BIT_ORDER       MSBFIRST
#define SPI_MODE            SPI_MODE0  // CPOL=0, CPHA=0

// ─── Timing Constants ───────────────────────────────────────────────────────

#define REPORT_INTERVAL_MS      10000UL  // Audio report every 10 seconds
#define HEARTBEAT_INTERVAL_MS   30000UL  // Heartbeat every 30 seconds
#define ACK_TIMEOUT_MS            500UL  // Wait 500ms for ACK before retry
#define MAX_RETRIES                   3  // Max SPI transmission retries
#define ARDUINO_UNRESPONSIVE_COUNT    3  // Consecutive failures → mark unresponsive
