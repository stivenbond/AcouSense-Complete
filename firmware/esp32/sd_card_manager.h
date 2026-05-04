/**
 * sd_card_manager.h
 * AcouSense ESP32 Firmware — SD Card Manager
 *
 * Manages SD card mounting, health checks, and directory initialization.
 * Other modules (DatabaseManager, WebServer) depend on this being mounted.
 *
 * Spec: docs/specs/04_esp32_firmware_spec.md §3.2
 */

#pragma once
#include <stdint.h>

enum class SDStatus : uint8_t {
    UNINITIALIZED = 0,
    OK            = 1,
    FAILED        = 2,
};

class SDCardManager {
public:
    /**
     * Initialize SPI bus for SD and attempt mount.
     * Call once in setup(). Safe to call even if no card is present
     * (will set status to FAILED and continue).
     */
    static void init();

    /** True if SD card is mounted and accessible. */
    static bool     isMounted();

    /** Current mount status. */
    static SDStatus getStatus();

    /**
     * Returns the root DB directory path.
     * Always "/acousense" — guaranteed to exist after a successful init().
     */
    static const char* getDataDir();

    /**
     * Returns the full path to the SQLite database file.
     * "/acousense/acousense.db"
     */
    static const char* getDBPath();

    /**
     * Returns the web assets directory path.
     * "/www" — served as static files by the web server.
     */
    static const char* getWWWDir();

    /**
     * Re-attempt mount. Call if a card is inserted after boot.
     * Returns true if newly mounted.
     */
    static bool remount();

private:
    static SDStatus _status;
    static bool     _ensureDirectories();
};
