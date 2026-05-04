/**
 * test_lcd.ino
 * AcouSense — Phase 1 Hardware Validation: I2C LCD Display
 *
 * Performs an I2C bus scan to detect the LCD backpack address,
 * then displays test content on the 16x2 LCD.
 *
 * Expected behavior:
 *   - Serial Monitor shows detected I2C address (usually 0x27 or 0x3F)
 *   - LCD displays a test message on both lines
 *   - LCD cycles through display states every 2 seconds
 *
 * Wiring:
 *   LCD SDA → Arduino A4
 *   LCD SCL → Arduino A5
 *   LCD VCC → 5V
 *   LCD GND → GND
 *
 * Library required: LiquidCrystal_I2C
 *   Install via: Arduino IDE → Sketch → Include Library → Manage Libraries
 *   Search for: "LiquidCrystal I2C" by Frank de Brabander
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Will be set after I2C scan — default to 0x27
uint8_t lcdAddress = 0x27;
LiquidCrystal_I2C* lcd = nullptr;

unsigned long lastUpdate  = 0;
uint8_t displayState = 0;

void scanI2C() {
    Serial.println(F("\n=== I2C Bus Scan ==="));
    bool found = false;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.print(F("Device found at address 0x"));
            Serial.println(addr, HEX);
            lcdAddress = addr;
            found = true;
        }
    }
    if (!found) {
        Serial.println(F("No I2C devices found! Check wiring."));
    }
    Serial.println(F("===================\n"));
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {}
    Serial.println(F("=== AcouSense: LCD Test ==="));

    Wire.begin();
    scanI2C();

    lcd = new LiquidCrystal_I2C(lcdAddress, 16, 2);
    lcd->init();
    lcd->backlight();

    lcd->setCursor(0, 0);
    lcd->print(F("AcouSense v1.0  "));
    lcd->setCursor(0, 1);
    lcd->print(F("LCD OK          "));

    Serial.print(F("LCD initialized at 0x"));
    Serial.println(lcdAddress, HEX);
    Serial.println(F("You should see text on the display."));
    Serial.println(F("If screen is blank, adjust contrast pot on backpack."));
}

void loop() {
    unsigned long now = millis();

    if (now - lastUpdate >= 2000) {
        lastUpdate = now;
        displayState = (displayState + 1) % 4;

        lcd->clear();
        switch (displayState) {
            case 0:
                lcd->setCursor(0, 0); lcd->print(F("AcouSense v1.0  "));
                lcd->setCursor(0, 1); lcd->print(F("LCD OK          "));
                break;
            case 1:
                lcd->setCursor(0, 0); lcd->print(F("Level:  072 dB  "));
                lcd->setCursor(0, 1); lcd->print(F("ALERT: LOW   OK "));
                break;
            case 2:
                lcd->setCursor(0, 0); lcd->print(F("Level:  091 dB  "));
                lcd->setCursor(0, 1); lcd->print(F("ALERT: HIGH  OK "));
                break;
            case 3:
                lcd->setCursor(0, 0); lcd->print(F("SPI: CONNECTED  "));
                lcd->setCursor(0, 1); lcd->print(F("SD:  OK   BT:ON "));
                break;
        }
        Serial.print(F("State ")); Serial.println(displayState);
    }
}
