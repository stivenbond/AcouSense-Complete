/**
 * test_sdcard.cpp
 * AcouSense — Phase 1 Hardware Validation: SD Card (ESP32)
 *
 * Uploads this sketch to the ESP32 (not the Arduino).
 * Tests SD card mounting, directory creation, file write, file read,
 * and content verification.
 *
 * Expected outcome:
 *   - Serial Monitor shows each step as PASS or FAIL
 *   - File /acousense/test.txt is created on the SD card
 *   - Content written matches content read back
 *
 * Wiring (ESP32 → SD Module):
 *   ESP32 GPIO13 → SD MOSI
 *   ESP32 GPIO12 → SD MISO
 *   ESP32 GPIO14 → SD SCK
 *   ESP32 GPIO15 → SD CS
 *   SD VCC → 3.3V rail
 *   SD GND → GND
 *
 * Library: SD.h (built-in with ESP32 Arduino Core)
 *
 * IMPORTANT: Use a properly formatted SD card (FAT32, ≤32GB).
 * Run SD card formatter before this test if mount fails.
 */

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

// SD card SPI pins (separate bus from Arduino communication)
#define SD_MOSI_PIN   13
#define SD_MISO_PIN   12
#define SD_SCK_PIN    14
#define SD_CS_PIN     15

#define TEST_DIR      "/acousense"
#define TEST_FILE     "/acousense/test.txt"
#define TEST_CONTENT  "AcouSense SD card validation - OK"

bool allPassed = true;

void logStep(const char* stepName, bool passed) {
    Serial.print(F("  ["));
    Serial.print(passed ? F("PASS") : F("FAIL"));
    Serial.print(F("] "));
    Serial.println(stepName);
    if (!passed) allPassed = false;
}

void setup() {
    Serial.begin(115200);
    delay(500);
    while (!Serial) {}

    Serial.println(F("=== AcouSense: SD Card Test (ESP32) ==="));
    Serial.println();

    // ── Step 1: Mount SD card ──────────────────────────────────────────────
    Serial.println(F("Step 1: Mount SD card"));
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    bool mounted = SD.begin(SD_CS_PIN);
    logStep("SD.begin() succeeded", mounted);

    if (!mounted) {
        Serial.println(F("\nSD card not detected. Check:"));
        Serial.println(F("  - Card is inserted"));
        Serial.println(F("  - Card is formatted as FAT32"));
        Serial.println(F("  - VCC connected to 3.3V"));
        Serial.println(F("  - Decoupling capacitor present"));
        Serial.println(F("  - Wiring matches spec (GPIO12/13/14/15)"));
        return;
    }

    // ── Step 2: Card info ──────────────────────────────────────────────────
    Serial.println(F("\nStep 2: Card information"));
    uint64_t cardSize = SD.cardSize() / (1024ULL * 1024ULL);
    Serial.print(F("  Card size: ")); Serial.print(cardSize); Serial.println(F(" MB"));
    Serial.print(F("  Used:      ")); Serial.print(SD.usedBytes() / 1024); Serial.println(F(" KB"));
    logStep("Card size > 0", cardSize > 0);

    // ── Step 3: Create directory ───────────────────────────────────────────
    Serial.println(F("\nStep 3: Create /acousense directory"));
    bool dirExists = SD.exists(TEST_DIR);
    if (!dirExists) {
        dirExists = SD.mkdir(TEST_DIR);
    }
    logStep("Directory /acousense exists or created", dirExists);

    // ── Step 4: Write file ─────────────────────────────────────────────────
    Serial.println(F("\nStep 4: Write test file"));
    File f = SD.open(TEST_FILE, FILE_WRITE);
    bool writeOk = false;
    if (f) {
        size_t written = f.print(TEST_CONTENT);
        f.close();
        writeOk = (written == strlen(TEST_CONTENT));
    }
    logStep("Write to /acousense/test.txt", writeOk);

    // ── Step 5: Read file back ─────────────────────────────────────────────
    Serial.println(F("\nStep 5: Read file back"));
    f = SD.open(TEST_FILE, FILE_READ);
    bool readOk = false;
    bool contentMatch = false;
    if (f) {
        char buf[64] = {0};
        size_t n = f.readBytes(buf, sizeof(buf) - 1);
        f.close();
        readOk = (n > 0);
        contentMatch = (strcmp(buf, TEST_CONTENT) == 0);
        Serial.print(F("  Read back: \"")); Serial.print(buf); Serial.println(F("\""));
    }
    logStep("Read from /acousense/test.txt", readOk);
    logStep("Content matches written data", contentMatch);

    // ── Step 6: Delete test file ───────────────────────────────────────────
    Serial.println(F("\nStep 6: Cleanup — delete test file"));
    bool deleted = SD.remove(TEST_FILE);
    logStep("Test file deleted", deleted);

    // ── Summary ───────────────────────────────────────────────────────────
    Serial.println();
    Serial.println(F("=============================="));
    if (allPassed) {
        Serial.println(F("ALL TESTS PASSED — SD card OK"));
    } else {
        Serial.println(F("SOME TESTS FAILED — check wiring/card"));
    }
    Serial.println(F("=============================="));
}

void loop() {
    // No loop needed for validation test
}
