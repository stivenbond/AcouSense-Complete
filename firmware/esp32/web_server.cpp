/**
 * web_server.cpp
 * AcouSense ESP32 Firmware — Web Server
 *
 * All REST handlers build JSON responses from DatabaseManager and the
 * live status of other modules. CSV export streams row-by-row.
 */

#include "web_server.h"
#include "database_manager.h"
#include "config_manager.h"
#include "sd_card_manager.h"
#include "spi_master_manager.h"
#include "bluetooth_manager.h"
#include <ESPAsyncWebServer.h>
#include <SD.h>
#include <WiFi.h>
#include <Arduino.h>
#include <ArduinoJson.h>   // ArduinoJson v7 — install via Library Manager

static AsyncWebServer server(80);

// ─── Helpers ─────────────────────────────────────────────────────────────────

static void sendJSON(AsyncWebServerRequest* req,
                     const String& json, int code = 200) {
    req->send(code, "application/json", json);
}

static void sendError(AsyncWebServerRequest* req,
                      const String& msg, int code = 500) {
    String j = "{\"error\":\"" + msg + "\"}";
    req->send(code, "application/json", j);
}

// ─── GET /api/status ─────────────────────────────────────────────────────────

static void handleStatus(AsyncWebServerRequest* req) {
    JsonDocument doc;

    doc["arduino"] = (SPIMasterManager::getStatus() == ArduinoStatus::ONLINE)
                     ? "online" : "offline";
    doc["sd_card"] = SDCardManager::isMounted() ? "ok" : "error";
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["bt_status"] = BluetoothManager::getStatusString();
    doc["last_sync_ts"] = (long long)BluetoothManager::getLastSyncTs();

    if (SPIMasterManager::getLastReportTime() > 0) {
        AudioReportPayload r = SPIMasterManager::getLastReport();
        JsonObject lr = doc["last_reading"].to<JsonObject>();
        lr["avg_level"]   = r.avg_level;
        lr["max_level"]   = r.max_level;
        lr["alert_level"] = r.alert_level;
    } else {
        doc["last_reading"] = nullptr;
    }

    String out;
    serializeJson(doc, out);
    sendJSON(req, out);
}

// ─── GET /api/readings ───────────────────────────────────────────────────────

struct ReadingsContext {
    JsonArray arr;
    int count = 0;
};

static void readingsCB(const NoiseRecord& rec, void* ud) {
    ReadingsContext* ctx = (ReadingsContext*)ud;
    JsonObject obj = ctx->arr.add<JsonObject>();
    obj["id"]          = (long long)rec.id;
    obj["timestamp"]   = (long long)rec.timestamp;
    obj["min_level"]   = rec.min_level;
    obj["max_level"]   = rec.max_level;
    obj["avg_level"]   = rec.avg_level;
    obj["alert_level"] = rec.alert_level;
    ctx->count++;
}

static void handleReadings(AsyncWebServerRequest* req) {
    int64_t fromTs = 0, toTs = 0;
    int limit = 200, offset = 0;

    if (req->hasParam("from"))   fromTs = atoll(req->getParam("from")->value().c_str());
    if (req->hasParam("to"))     toTs   = atoll(req->getParam("to")->value().c_str());
    if (req->hasParam("limit"))  limit  = req->getParam("limit")->value().toInt();
    if (req->hasParam("offset")) offset = req->getParam("offset")->value().toInt();

    int total = DatabaseManager::countReadings(fromTs, toTs);

    JsonDocument doc;
    doc["total"]  = total;
    doc["limit"]  = limit;
    doc["offset"] = offset;
    JsonArray arr = doc["data"].to<JsonArray>();

    ReadingsContext ctx { arr, 0 };
    DatabaseManager::getReadings(fromTs, toTs, limit, offset, readingsCB, &ctx);

    String out;
    serializeJson(doc, out);
    sendJSON(req, out);
}

// ─── GET /api/readings/export (CSV) ──────────────────────────────────────────

static void handleExport(AsyncWebServerRequest* req) {
    int64_t fromTs = 0, toTs = 0;
    if (req->hasParam("from")) fromTs = atoll(req->getParam("from")->value().c_str());
    if (req->hasParam("to"))   toTs   = atoll(req->getParam("to")->value().c_str());

    // Stream CSV using chunked response
    AsyncResponseStream* resp = req->beginResponseStream("text/csv");
    resp->addHeader("Content-Disposition", "attachment; filename=\"acousense_export.csv\"");
    resp->print("id,timestamp,min_level,max_level,avg_level,alert_level\n");

    DatabaseManager::getReadings(fromTs, toTs, 0, 0,
        [](const NoiseRecord& rec, void* ud) {
            AsyncResponseStream* r = (AsyncResponseStream*)ud;
            char row[80];
            snprintf(row, sizeof(row), "%lld,%lld,%d,%d,%d,%d\n",
                     rec.id, rec.timestamp,
                     rec.min_level, rec.max_level,
                     rec.avg_level, rec.alert_level);
            r->print(row);
        }, resp);

    req->send(resp);
}

// ─── GET /api/config ─────────────────────────────────────────────────────────

static void handleGetConfig(AsyncWebServerRequest* req) {
    const ESPConfig& cfg = ESP32ConfigManager::get();
    JsonDocument doc;
    doc["low_threshold"]    = cfg.low_threshold;
    doc["medium_threshold"] = cfg.medium_threshold;
    doc["high_threshold"]   = cfg.high_threshold;
    doc["pattern_low"]      = cfg.pattern_low;
    doc["pattern_medium"]   = cfg.pattern_medium;
    doc["pattern_high"]     = cfg.pattern_high;

    String out;
    serializeJson(doc, out);
    sendJSON(req, out);
}

// ─── POST /api/config ────────────────────────────────────────────────────────

static void handlePostConfig(AsyncWebServerRequest* req, uint8_t* data,
                              size_t len, size_t index, size_t total) {
    if (index + len < total) return;  // Wait for full body

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, (const char*)data, len);
    if (err) {
        sendError(req, "Invalid JSON", 400);
        return;
    }

    ESPConfig cfg;
    cfg.low_threshold    = doc["low_threshold"]    | 40;
    cfg.medium_threshold = doc["medium_threshold"] | 60;
    cfg.high_threshold   = doc["high_threshold"]   | 80;
    cfg.pattern_low      = doc["pattern_low"]      | 1;
    cfg.pattern_medium   = doc["pattern_medium"]   | 2;
    cfg.pattern_high     = doc["pattern_high"]     | 3;

    // Validate threshold ordering
    if (cfg.low_threshold >= cfg.medium_threshold ||
        cfg.medium_threshold >= cfg.high_threshold) {
        sendError(req, "Thresholds must satisfy low < medium < high", 400);
        return;
    }

    bool saved = ESP32ConfigManager::apply(cfg);  // saves DB + pushes SPI

    JsonDocument resp;
    resp["success"]       = saved;
    resp["arduino_synced"] = (SPIMasterManager::getStatus() == ArduinoStatus::ONLINE);
    String out;
    serializeJson(resp, out);
    sendJSON(req, out);
}

// ─── GET /api/devices ────────────────────────────────────────────────────────

static void handleDevices(AsyncWebServerRequest* req) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    long long syncWindowStart = (long long)(millis() / 1000UL) - 300; // 5 min ago

    DatabaseManager::getDevices([](const BTDevice& dev, void* ud) {
        JsonArray* a = (JsonArray*)ud;
        JsonObject obj = a->add<JsonObject>();
        obj["id"]              = (long long)dev.id;
        obj["mac_address"]     = dev.mac_address;
        obj["user_identifier"] = dev.user_identifier;
        obj["last_seen"]       = (long long)dev.last_seen;
    }, &arr);

    String out;
    serializeJson(doc, out);
    sendJSON(req, out);
}

// ─── Init ─────────────────────────────────────────────────────────────────────

void AcouWebServer::init() {
    // Serve static files from /www on SD card
    server.serveStatic("/", SD, SDCardManager::getWWWDir())
          .setDefaultFile("index.html");

    // REST API routes
    server.on("/api/status",   HTTP_GET,  handleStatus);
    server.on("/api/readings", HTTP_GET,  handleReadings);
    server.on("/api/readings/export", HTTP_GET, handleExport);
    server.on("/api/config",   HTTP_GET,  handleGetConfig);
    server.on("/api/devices",  HTTP_GET,  handleDevices);

    // POST /api/config — body handler
    server.on("/api/config", HTTP_POST,
        [](AsyncWebServerRequest* req) {},  // request handler (empty — body handler fires)
        nullptr,
        handlePostConfig
    );

    // 404 fallback — return index.html for SPA client-side routing
    server.onNotFound([](AsyncWebServerRequest* req) {
        if (req->url().startsWith("/api/")) {
            req->send(404, "application/json", "{\"error\":\"not found\"}");
        } else {
            req->send(SD, "/www/index.html", "text/html");
        }
    });

    server.begin();
    Serial.println(F("[Web] Server started on port 80"));
    Serial.print(F("[Web] Dashboard: http://"));
    Serial.println(WiFi.localIP());
}
