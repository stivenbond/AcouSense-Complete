/**
 * ble_constants.h
 * AcouSense — Shared BLE Service & Characteristic UUIDs
 *
 * These UUIDs must be identical on the ESP32 and the Android application.
 * Any change here must be propagated to both.
 *
 * Spec: docs/specs/07_bluetooth_sync_spec.md
 */

#pragma once

// ─── AcouSense BLE Service UUID ─────────────────────────────────────────────

#define ACOUSENSE_SERVICE_UUID          "0000AC00-0000-1000-8000-00805F9B34FB"

// ─── Characteristic UUIDs ───────────────────────────────────────────────────

/**
 * Sync Payload Characteristic
 * ESP32 writes 52-byte SyncPayload binary struct to this characteristic.
 * Android GATT server: Write Without Response
 */
#define ACOUSENSE_CHAR_SYNC_PAYLOAD     "0000AC01-0000-1000-8000-00805F9B34FB"

/**
 * Device Registration Characteristic
 * Android app writes user_identifier (32-byte null-padded string) on pairing.
 * ESP32 GATT server (pairing mode): Read + Write
 */
#define ACOUSENSE_CHAR_REGISTRATION     "0000AC02-0000-1000-8000-00805F9B34FB"

/**
 * Acknowledge Characteristic
 * Android notifies ESP32 after successfully storing a sync payload.
 * Android GATT server: Notify
 */
#define ACOUSENSE_CHAR_ACK              "0000AC03-0000-1000-8000-00805F9B34FB"

// ─── Sync Payload Structure ──────────────────────────────────────────────────
// Total: 52 bytes. Transmitted as raw binary over ACOUSENSE_CHAR_SYNC_PAYLOAD.

#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint8_t  device_id[6];      // ESP32 MAC address (raw bytes)
    uint8_t  user_id[32];       // user_identifier, null-padded ASCII string
    uint32_t ts_start;          // Period start (Unix timestamp, seconds)
    uint32_t ts_end;            // Period end (Unix timestamp, seconds)
    uint16_t avg_noise;         // Average normalized noise level (0-100)
    uint16_t peak_noise;        // Peak normalized noise level (0-100)
    uint8_t  exposure_class;    // 0=safe, 1=caution, 2=moderate, 3=high, 4=danger
    uint8_t  reserved;          // Align to even byte boundary
} SyncPayload;

// ─── BLE Timing ─────────────────────────────────────────────────────────────

#define BLE_SYNC_CYCLE_MS       300000UL  // 5-minute sync cycle
#define BLE_CONNECT_TIMEOUT_MS   10000UL  // 10s connection timeout per device
#define BLE_WRITE_TIMEOUT_MS      5000UL  // 5s write timeout after connection
#define BLE_ACK_TIMEOUT_MS        5000UL  // 5s ACK notify timeout
