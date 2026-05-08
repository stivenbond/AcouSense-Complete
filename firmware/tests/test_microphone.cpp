/**
 * test_microphone.cpp
 * AcouSense — Phase 1 Hardware Validation: Microphone Sound Sensor
 *
 * Reads the analog output of the sound sensor on A0.
 * Prints raw ADC value and normalized level (0-100) to Serial Monitor.
 *
 * Expected behavior:
 *   - In silence: reads a consistent baseline (~200-400 ADC)
 *   - On sound (clap, speak): value spikes significantly
 *   - Normalized level should visibly change with sound input
 *
 * Wiring:
 *   Sound sensor AO → Arduino A0
 *   Sound sensor VCC → 5V
 *   Sound sensor GND → GND
 */
#include <Arduino.h>

#define MIC_PIN        A0
#define SAMPLE_RATE_MS 50     // Sample every 50ms (20 Hz)
#define PRINT_RATE_MS  500    // Print summary every 500ms

unsigned long lastSampleTime = 0;
unsigned long lastPrintTime  = 0;

int rawMin = 1023;
int rawMax = 0;
long rawSum = 0;
int sampleCount = 0;

// Normalize a raw ADC value (0–1023) to a 0–100 scale
uint8_t normalize(int raw) {
    return (uint8_t)((long)raw * 100 / 1023);
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {}
    Serial.println(F("=== AcouSense: Microphone Test ==="));
    Serial.println(F("Make noise or stay quiet to see readings change."));
    Serial.println(F("Format: RAW_MIN | RAW_MAX | RAW_AVG | NORM_AVG"));
}

void loop() {
    unsigned long now = millis();

    // Sample at 20 Hz
    if (now - lastSampleTime >= SAMPLE_RATE_MS) {
        lastSampleTime = now;
        int raw = analogRead(MIC_PIN);
        if (raw < rawMin) rawMin = raw;
        if (raw > rawMax) rawMax = raw;
        rawSum += raw;
        sampleCount++;
    }

    // Print summary every 500ms
    if (now - lastPrintTime >= PRINT_RATE_MS && sampleCount > 0) {
        lastPrintTime = now;
        int rawAvg = (int)(rawSum / sampleCount);

        Serial.print(F("MIN: ")); Serial.print(rawMin);
        Serial.print(F("  MAX: ")); Serial.print(rawMax);
        Serial.print(F("  AVG: ")); Serial.print(rawAvg);
        Serial.print(F("  NORM: ")); Serial.print(normalize(rawAvg));
        Serial.println(F("/100"));

        // Reset for next window
        rawMin = 1023;
        rawMax = 0;
        rawSum = 0;
        sampleCount = 0;
    }
}
