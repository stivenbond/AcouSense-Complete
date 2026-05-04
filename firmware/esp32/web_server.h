/**
 * web_server.h
 * AcouSense ESP32 Firmware — Web Server
 *
 * Serves the SvelteKit dashboard as static files from SD /www/ and
 * exposes all 6 REST API endpoints.
 *
 * Library: ESPAsyncWebServer + AsyncTCP
 *   Install: Library Manager → "ESPAsyncWebServer" by me-no-dev
 *
 * Spec: docs/specs/04_esp32_firmware_spec.md §3.5
 *       docs/specs/06_web_dashboard_spec.md §5
 */

#pragma once

class AcouWebServer {
public:
    /**
     * Initialize and start the web server on port 80.
     * Must be called after WiFi is connected and DatabaseManager is ready.
     */
    static void init();

    /**
     * No update loop needed — ESPAsyncWebServer is fully async/interrupt-driven.
     * This is a no-op kept for API symmetry.
     */
    static void update() {}
};
