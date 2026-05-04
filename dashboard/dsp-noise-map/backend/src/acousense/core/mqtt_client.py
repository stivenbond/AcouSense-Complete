"""
AcouSense — MQTT Subscriber
=============================
Decoupled MQTT client that runs in its own daemon thread and feeds
incoming sensor payloads into the persistence layer.

Features:
  • Auto-reconnect with exponential back-off (max 60 s)
  • Paho MQTT CallbackAPIVersion.VERSION2 (paho-mqtt ≥ 2.0)
  • Graceful degradation when paho-mqtt is not installed
  • Topic pattern support (wildcard ``noisemap/+/sensor`` etc.)
  • Security audit logging on connect/disconnect/auth failures
  • Credentials loaded from OS keyring (never from .env)
"""

from __future__ import annotations

import json
import os
import threading
import time
from typing import Any

from src.persistence.database import insert_reading

# ─────────────────────────────────────────────────────────────────────────────
# Audit logger (graceful if unavailable)
# ─────────────────────────────────────────────────────────────────────────────
try:
    from src.security.audit import audit_log

    _HAS_AUDIT = True
except ImportError:
    _HAS_AUDIT = False

# ─────────────────────────────────────────────────────────────────────────────
# Secret management — load MQTT credentials from keyring
# ─────────────────────────────────────────────────────────────────────────────
try:
    from src.security.secrets import get_secret

    _HAS_SECRETS = True
except ImportError:
    _HAS_SECRETS = False

# ─────────────────────────────────────────────────────────────────────────────
# Configuration (keyring first, then env fallback for non-sensitive settings)
# ─────────────────────────────────────────────────────────────────────────────
MQTT_BROKER: str = (
    get_secret("MQTT_BROKER") if _HAS_SECRETS else None
) or os.environ.get("MQTT_BROKER", "localhost")
MQTT_PORT: int = int(os.environ.get("MQTT_PORT", "1883"))
MQTT_TOPIC: str = os.environ.get("MQTT_TOPIC", "noisemap/sensor")
MQTT_KEEPALIVE: int = int(os.environ.get("MQTT_KEEPALIVE", "60"))
MQTT_RECONNECT_MAX_DELAY: int = 60  # seconds

# Sensitive credentials — ONLY from keyring, never from environment
MQTT_USERNAME: str | None = get_secret("MQTT_USERNAME") if _HAS_SECRETS else None
MQTT_PASSWORD: str | None = get_secret("MQTT_PASSWORD") if _HAS_SECRETS else None

# ─────────────────────────────────────────────────────────────────────────────
# Optional import
# ─────────────────────────────────────────────────────────────────────────────
try:
    import paho.mqtt.client as _mqtt  # type: ignore[import]

    HAS_MQTT = True
except ImportError:
    _mqtt = None  # type: ignore[assignment]
    HAS_MQTT = False

# ─────────────────────────────────────────────────────────────────────────────
# Stats (visible to TUI / diagnostics)
# ─────────────────────────────────────────────────────────────────────────────
_stats_lock = threading.Lock()
_stats: dict[str, Any] = {
    "connected": False,
    "messages_received": 0,
    "messages_rejected": 0,
    "last_message_at": None,
    "broker": f"{MQTT_BROKER}:{MQTT_PORT}",
}


def get_mqtt_stats() -> dict[str, Any]:
    """Return a snapshot of real-time MQTT statistics."""
    with _stats_lock:
        return dict(_stats)


# ─────────────────────────────────────────────────────────────────────────────
# Callbacks
# ─────────────────────────────────────────────────────────────────────────────


def _on_connect(
    client: Any,
    userdata: Any,
    flags: Any,
    reason_code: Any,
    properties: Any = None,
) -> None:
    with _stats_lock:
        _stats["connected"] = reason_code == 0
    if reason_code == 0:
        print(f"[mqtt] connected to {MQTT_BROKER}:{MQTT_PORT}")
        client.subscribe(MQTT_TOPIC)
        print(f"[mqtt] subscribed to '{MQTT_TOPIC}'")
        if _HAS_AUDIT:
            audit_log.mqtt_connected(MQTT_BROKER, MQTT_PORT)
    else:
        print(f"[mqtt] connection failed: rc={reason_code}")
        if _HAS_AUDIT:
            audit_log.mqtt_auth_failure(int(reason_code))


def _on_disconnect(
    client: Any,
    userdata: Any,
    disconnect_flags: Any,
    reason_code: Any,
    properties: Any = None,
) -> None:
    with _stats_lock:
        _stats["connected"] = False
    print(f"[mqtt] disconnected: rc={reason_code}")
    if _HAS_AUDIT:
        audit_log.mqtt_disconnected(int(reason_code))


def _on_message(client: Any, userdata: Any, msg: Any) -> None:
    try:
        payload: dict[str, Any] = json.loads(msg.payload.decode("utf-8"))
        ok = insert_reading(payload)
        with _stats_lock:
            if ok:
                _stats["messages_received"] += 1
                _stats["last_message_at"] = time.time()
            else:
                _stats["messages_rejected"] += 1
                if _HAS_AUDIT:
                    audit_log.payload_rejected(msg.topic, "missing_required_fields")
    except (json.JSONDecodeError, KeyError, UnicodeDecodeError) as exc:
        print(f"[mqtt] bad payload on {msg.topic}: {exc}")
        with _stats_lock:
            _stats["messages_rejected"] += 1
        if _HAS_AUDIT:
            audit_log.payload_rejected(msg.topic, str(type(exc).__name__))


# ─────────────────────────────────────────────────────────────────────────────
# Public start function
# ─────────────────────────────────────────────────────────────────────────────


def start_mqtt_loop() -> None:
    """
    Connect to the MQTT broker and begin the blocking network loop.
    Reconnects using exponential back-off on failure.
    Intended to run inside a daemon thread.

    Credentials are loaded from the OS keyring.  If ``MQTT_USERNAME`` and
    ``MQTT_PASSWORD`` are set in the keyring, TLS authentication is used.
    """
    if not HAS_MQTT:
        print("[mqtt] paho-mqtt not installed — MQTT ingest disabled.")
        return

    client = _mqtt.Client(_mqtt.CallbackAPIVersion.VERSION2)
    client.on_connect = _on_connect
    client.on_disconnect = _on_disconnect
    client.on_message = _on_message

    # Apply credentials from keyring if configured
    if MQTT_USERNAME and MQTT_PASSWORD:
        client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
        print("[mqtt] using keyring-sourced credentials for authentication")

    delay = 2.0
    while True:
        try:
            client.connect(MQTT_BROKER, MQTT_PORT, MQTT_KEEPALIVE)
            delay = 2.0  # reset back-off on successful connect
            client.loop_forever()
        except Exception as exc:
            print(f"[mqtt] broker unreachable ({exc}). Retry in {delay:.0f}s …")
            time.sleep(delay)
            delay = min(delay * 2, MQTT_RECONNECT_MAX_DELAY)


def start_mqtt_thread() -> threading.Thread:
    """
    Spawn and return the MQTT daemon thread.
    The thread is a daemon so it exits when the main process exits.
    """
    t = threading.Thread(target=start_mqtt_loop, name="acousense-mqtt", daemon=True)
    t.start()
    return t
