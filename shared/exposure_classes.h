/**
 * exposure_classes.h
 * AcouSense — Exposure Classification Thresholds
 *
 * Based on WHO noise exposure guidelines.
 * Used by: ESP32 SyncManager, Android recommendation engine.
 *
 * Spec: docs/specs/07_bluetooth_sync_spec.md §8
 *       docs/specs/08_android_app_spec.md §8
 */

#pragma once
#include <stdint.h>

// ─── Exposure Class IDs ─────────────────────────────────────────────────────

#define EXPOSURE_SAFE       0   // avg_noise < 40
#define EXPOSURE_CAUTION    1   // 40 <= avg_noise < 60
#define EXPOSURE_MODERATE   2   // 60 <= avg_noise < 75
#define EXPOSURE_HIGH       3   // 75 <= avg_noise < 90
#define EXPOSURE_DANGER     4   // avg_noise >= 90

// ─── Classification Boundaries (normalized 0-100 scale) ─────────────────────

#define EXPOSURE_THRESHOLD_CAUTION    40
#define EXPOSURE_THRESHOLD_MODERATE   60
#define EXPOSURE_THRESHOLD_HIGH       75
#define EXPOSURE_THRESHOLD_DANGER     90

// ─── Classifier Function ────────────────────────────────────────────────────

/**
 * Returns the exposure class ID (0-4) for a given avg_noise value (0-100).
 */
static inline uint8_t classifyExposure(uint16_t avg_noise) {
    if (avg_noise < EXPOSURE_THRESHOLD_CAUTION)  return EXPOSURE_SAFE;
    if (avg_noise < EXPOSURE_THRESHOLD_MODERATE) return EXPOSURE_CAUTION;
    if (avg_noise < EXPOSURE_THRESHOLD_HIGH)     return EXPOSURE_MODERATE;
    if (avg_noise < EXPOSURE_THRESHOLD_DANGER)   return EXPOSURE_HIGH;
    return EXPOSURE_DANGER;
}

// ─── Human-Readable Labels ──────────────────────────────────────────────────

static const char* const EXPOSURE_CLASS_LABELS[] = {
    "Safe",
    "Caution",
    "Moderate",
    "High",
    "Danger"
};
