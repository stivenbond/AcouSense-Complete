/**
 * PlatformIO dependencies:
 * kosme/arduinoFFT @ ^2.0.1
 * bblanchon/ArduinoJson @ ^6.21.3
 * knolleary/PubSubClient @ ^2.8
 * mikalhart/TinyGPSPlus @ ^1.0.3
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>
#include <ArduinoJson.h>
#include <TinyGPSPlus.h>

#define WIFI_SSID "YOUR_SSID"
#define WIFI_PASSWORD "YOUR_PASSWORD"
#define MQTT_BROKER "192.168.1.100"
#define MQTT_PORT 1883
#define MQTT_TOPIC "noisemap/sensor"
#define SENSOR_ID "sensor-01"
#define FIXED_LAT 41.3275
#define FIXED_LNG 19.8187
#define I2S_SD 32
#define I2S_WS 25
#define I2S_SCK 26
#define I2S_PORT I2S_NUM_0
#define GPS_UART Serial2
#define GPS_BAUD 9600
#define SAMPLE_RATE 44100
#define FFT_SIZE 1024
#define WINDOW_SIZE FFT_SIZE
#define LEQ_WINDOW_SEC 30
#define MIC_REF_DB 94.0f
#define MIC_REF_AMPL 32768.0f
#define MIC_OFFSET_DB 0.0f

static const float A_WEIGHT_FREQS[] = {10, 12.5, 16, 20, 25, 31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000, 20000};
static const float A_WEIGHT_DB[] = {-70.4, -63.4, -56.7, -50.5, -44.7, -39.4, -34.6, -30.2, -26.2, -22.5, -19.1, -16.1, -13.4, -10.9, -8.6, -6.6, -4.8, -3.2, -1.9, -0.8, 0.0, 0.6, 1.0, 1.2, 1.3, 1.2, 1.0, 0.5, -0.1, -1.1, -2.5, -4.3, -6.6, -9.3};

double vReal[FFT_SIZE];
double vImag[FFT_SIZE];
arduinoFFT FFT(vReal, vImag, FFT_SIZE, SAMPLE_RATE);

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
TinyGPSPlus gps;

float leqAccumulator = 0.0f;
int leqSampleCount = 0;
unsigned long lastPublishMs = 0;

float aWeightForFreq(float freq) {
    if (freq <= A_WEIGHT_FREQS[0]) return A_WEIGHT_DB[0];
    int numFreqs = sizeof(A_WEIGHT_FREQS) / sizeof(A_WEIGHT_FREQS[0]);
    if (freq >= A_WEIGHT_FREQS[numFreqs - 1]) return A_WEIGHT_DB[numFreqs - 1];

    for (int i = 0; i < numFreqs - 1; i++) {
        if (freq >= A_WEIGHT_FREQS[i] && freq <= A_WEIGHT_FREQS[i + 1]) {
            float t = (freq - A_WEIGHT_FREQS[i]) / (A_WEIGHT_FREQS[i + 1] - A_WEIGHT_FREQS[i]);
            return A_WEIGHT_DB[i] + t * (A_WEIGHT_DB[i + 1] - A_WEIGHT_DB[i]);
        }
    }
    return 0.0f;
}

void i2sInit() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
        .use_apll = true,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD
    };
    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pin_config);
}

float processFrame() {
    int32_t samples[FFT_SIZE];
    size_t bytesRead;
    i2s_read(I2S_PORT, &samples, sizeof(samples), &bytesRead, portMAX_DELAY);
    
    for (int i = 0; i < FFT_SIZE; i++) {
        float sample = (float)(samples[i] >> 8);
        sample /= MIC_REF_AMPL;
        float multiplier = 0.5f * (1.0f - cos(2.0f * PI * i / (FFT_SIZE - 1)));
        vReal[i] = sample * multiplier;
        vImag[i] = 0.0f;
    }
    
    FFT.Compute(FFT_FORWARD);
    FFT.ComplexToMagnitude();
    
    float energyLinear = 0.0f;
    for (int i = 1; i < FFT_SIZE / 2; i++) {
        float freq = (float)i * SAMPLE_RATE / FFT_SIZE;
        float mag = vReal[i] * 2.0f / FFT_SIZE;
        float magDB = 20.0f * log10(mag + 1e-12f) + MIC_REF_DB - 20.0f * log10(MIC_REF_AMPL) + MIC_OFFSET_DB;
        // For dB(Z), we do not apply A-weighting (flat response)
        energyLinear += pow(10.0f, magDB / 10.0f);
    }
    
    leqAccumulator += energyLinear;
    leqSampleCount++;
    
    return 10.0f * log10(energyLinear + 1e-12f);
}

void publishLeq() {
    if (leqSampleCount == 0) return;
    float leq = 10.0f * log10(leqAccumulator / leqSampleCount);
    leqAccumulator = 0.0f;
    leqSampleCount = 0;
    
    float lat = FIXED_LAT;
    float lng = FIXED_LNG;
    int gpsValid = 0;
    
    if (gps.location.isValid() && gps.location.age() < 5000) {
        lat = gps.location.lat();
        lng = gps.location.lng();
        gpsValid = 1;
    }
    
    StaticJsonDocument<256> doc;
    doc["sensor_id"] = SENSOR_ID;
    doc["ts"] = millis();
    doc["leq_dbz"] = round(leq * 10.0) / 10.0;
    doc["lat"] = lat;
    doc["lng"] = lng;
    doc["gps_valid"] = gpsValid;
    
    char payload[256];
    serializeJson(doc, payload);
    
    mqtt.publish(MQTT_TOPIC, payload, true);
    Serial.println(payload);
}

void connectWiFi() {
    Serial.print("Connecting to WiFi");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
}

void connectMQTT() {
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    while (!mqtt.connected()) {
        Serial.print("Connecting to MQTT...");
        if (mqtt.connect(SENSOR_ID)) {
            Serial.println("connected");
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqtt.state());
            Serial.println(" retrying in 5s");
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    GPS_UART.begin(GPS_BAUD, SERIAL_8N1, 16, 17);
    i2sInit();
    connectWiFi();
    connectMQTT();
    lastPublishMs = millis();
}

void loop() {
    while (GPS_UART.available() > 0) {
        gps.encode(GPS_UART.read());
    }
    
    if (!mqtt.connected()) {
        connectMQTT();
    }
    mqtt.loop();
    
    float spl = processFrame();
    Serial.print("SPL: ");
    Serial.println(spl);
    
    if (millis() - lastPublishMs >= LEQ_WINDOW_SEC * 1000) {
        publishLeq();
        lastPublishMs = millis();
    }
}
