/**
 * test_buzzer.cpp
 * AcouSense — Phase 1 Hardware Validation: Passive Buzzer
 *
 * Tests the passive buzzer on D9 using tone() / noTone().
 * Cycles through 4 patterns defined in the firmware spec.
 *
 * Expected behavior:
 *   - Each pattern produces a distinct audible sound
 *   - Pattern 0x01: single short beep every 3 seconds
 *   - Pattern 0x02: double pulse repeating
 *   - Pattern 0x03: rapid alarm
 *   - Serial Monitor describes the current pattern
 *
 * Wiring:
 *   Buzzer (+) → Arduino D9
 *   Buzzer (-) → GND
 *
 * IMPORTANT: This sketch requires a PASSIVE buzzer.
 * An active buzzer will not respond correctly to frequency changes.
 */

#define BUZZER_PIN     9
#define TONE_FREQ   2000    // Hz — standard warning tone

// Pattern state machine
struct PatternState {
    unsigned long lastTick;
    uint8_t step;
};

PatternState ps = {0, 0};
uint8_t currentPattern = 0;
unsigned long patternStartTime = 0;
#define PATTERN_DURATION_MS 6000  // Run each pattern for 6 seconds

// ─── Pattern 0x01: Single beep every 3 seconds ───────────────────────────────
void runPattern01(unsigned long now) {
    // ON 100ms → OFF 2900ms
    uint32_t t = (now - patternStartTime) % 3000;
    if (t < 100) tone(BUZZER_PIN, TONE_FREQ);
    else         noTone(BUZZER_PIN);
}

// ─── Pattern 0x02: Double pulse every 2 seconds ──────────────────────────────
void runPattern02(unsigned long now) {
    // ON 100ms → OFF 200ms → ON 100ms → OFF 1600ms
    uint32_t t = (now - patternStartTime) % 2000;
    if      (t < 100)  tone(BUZZER_PIN, TONE_FREQ);
    else if (t < 300)  noTone(BUZZER_PIN);
    else if (t < 400)  tone(BUZZER_PIN, TONE_FREQ);
    else               noTone(BUZZER_PIN);
}

// ─── Pattern 0x03: Rapid continuous alarm ────────────────────────────────────
void runPattern03(unsigned long now) {
    // ON 80ms → OFF 80ms
    uint32_t t = (now - patternStartTime) % 160;
    if (t < 80) tone(BUZZER_PIN, TONE_FREQ);
    else        noTone(BUZZER_PIN);
}

const char* patternNames[] = {
    "0x00 SILENT",
    "0x01 Single beep every 3s",
    "0x02 Double pulse",
    "0x03 Rapid alarm"
};

void setup() {
    Serial.begin(115200);
    while (!Serial) {}
    pinMode(BUZZER_PIN, OUTPUT);
    Serial.println(F("=== AcouSense: Buzzer Test ==="));
    Serial.println(F("Cycling through all 4 buzzer patterns (6 seconds each)"));
    patternStartTime = millis();
    Serial.print(F("Pattern: ")); Serial.println(patternNames[0]);
}

void loop() {
    unsigned long now = millis();

    // Advance to next pattern every PATTERN_DURATION_MS
    if (now - patternStartTime >= PATTERN_DURATION_MS) {
        noTone(BUZZER_PIN);
        currentPattern = (currentPattern + 1) % 4;
        patternStartTime = now;
        Serial.print(F("Pattern: ")); Serial.println(patternNames[currentPattern]);
    }

    switch (currentPattern) {
        case 0: noTone(BUZZER_PIN); break;
        case 1: runPattern01(now);  break;
        case 2: runPattern02(now);  break;
        case 3: runPattern03(now);  break;
    }
}
