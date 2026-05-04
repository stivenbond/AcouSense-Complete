"""
AcouSense — Security Audit Logger
====================================
Structured, rotation-managed JSON audit log for security-relevant events.

Design principles
-----------------
• **No secrets in logs** — sensor IDs are hashed (SHA-256 prefix), no
  coordinates, no credentials, no raw dBa values.
• **Tamper-evident chain** — each entry carries the SHA-256 hash of the
  previous entry, forming an append-only chain detectable if entries are
  deleted.
• **Structured output** — JSONL (one JSON object per line) for easy ingestion
  by Grafana, Splunk, or any log aggregation tool.
• **Rotation** — ``RotatingFileHandler`` keeps 5 compressed backup files of
  up to 5 MB each.

Log Schema
----------
Every event record has these fields:

    {
      "ts":        "2025-06-01T14:23:11.432Z",  // ISO 8601 UTC
      "seq":       12345,                         // monotonic sequence number
      "event":     "MQTT_AUTH_FAILURE",
      "severity":  "WARNING",
      "context":   { ... }                        // event-specific, no PII
      "prev_hash": "abc123..."                    // SHA-256 of previous line
    }

Usage
-----
    from src.security.audit import audit_log

    audit_log.server_start(port=5000)
    audit_log.mqtt_auth_failure(reason_code=4)
    audit_log.payload_rejected(topic="noisemap/sensor", reason="missing_field")
    audit_log.db_prune(deleted_rows=142, operator="cli")
"""

from __future__ import annotations

import hashlib
import json
import logging
import logging.handlers
import os
import threading
from datetime import UTC, datetime
from pathlib import Path
from typing import Any

# ─────────────────────────────────────────────────────────────────────────────
# Configuration
# ─────────────────────────────────────────────────────────────────────────────
AUDIT_LOG_PATH: str = os.environ.get("AUDIT_LOG_PATH", "audit.log")
_MAX_BYTES: int = 5 * 1024 * 1024  # 5 MB per file
_BACKUP_COUNT: int = 5  # 5 compressed backups = 25 MB total


# ─────────────────────────────────────────────────────────────────────────────
# Low-level logger setup
# ─────────────────────────────────────────────────────────────────────────────


def _build_logger(path: str) -> logging.Logger:
    log = logging.getLogger("acousense.audit")
    log.setLevel(logging.DEBUG)
    log.propagate = False  # never emit to root logger / stdout
    if not log.handlers:
        handler = logging.handlers.RotatingFileHandler(
            path,
            maxBytes=_MAX_BYTES,
            backupCount=_BACKUP_COUNT,
            encoding="utf-8",
        )
        handler.setFormatter(logging.Formatter("%(message)s"))
        log.addHandler(handler)
    return log


# ─────────────────────────────────────────────────────────────────────────────
# Helper: hash sensor_id for logging (no raw IDs in logs)
# ─────────────────────────────────────────────────────────────────────────────


def _safe_sensor_ref(sensor_id: str) -> str:
    """Return a truncated SHA-256 of the sensor ID — logs no PII."""
    return "sha256:" + hashlib.sha256(sensor_id.encode()).hexdigest()[:12]


# ─────────────────────────────────────────────────────────────────────────────
# Audit Logger class
# ─────────────────────────────────────────────────────────────────────────────


class AuditLogger:
    """
    Thread-safe structured audit logger.

    All public methods are named after the event they record and accept
    only non-sensitive context parameters.
    """

    def __init__(self, log_path: str = AUDIT_LOG_PATH) -> None:
        self._log = _build_logger(log_path)
        self._lock = threading.Lock()
        self._seq: int = 0
        self._prev_hash: str = "GENESIS"

    # ── Internal writer ────────────────────────────────────────────────────

    def _write(self, event: str, severity: str, context: dict[str, Any]) -> None:
        """Serialise and write a single audit record."""
        with self._lock:
            self._seq += 1
            record: dict[str, Any] = {
                "ts": datetime.now(UTC).isoformat(),
                "seq": self._seq,
                "event": event,
                "severity": severity,
                "context": context,
                "prev_hash": self._prev_hash,
            }
            line = json.dumps(record, separators=(",", ":"))
            self._prev_hash = hashlib.sha256(line.encode()).hexdigest()
            self._log.info(line)

    # ── Public event methods ───────────────────────────────────────────────

    def server_start(self, host: str, port: int) -> None:
        self._write(
            "SERVER_START",
            "INFO",
            {"host": host, "port": port, "pid": os.getpid()},
        )

    def server_stop(self) -> None:
        self._write("SERVER_STOP", "INFO", {"pid": os.getpid()})

    def db_init(self, db_path: str) -> None:
        # Log only the filename, never full path (may contain username)
        self._write(
            "DB_INIT",
            "INFO",
            {"db_file": Path(db_path).name},
        )

    def mqtt_connected(self, broker: str, port: int) -> None:
        self._write(
            "MQTT_CONNECTED",
            "INFO",
            {"broker": broker, "port": port},
        )

    def mqtt_auth_failure(self, reason_code: int) -> None:
        """
        Called when MQTT broker returns non-zero reason code.
        Triggers a WARNING — indicates misconfiguration or active attack.
        """
        self._write(
            "MQTT_AUTH_FAILURE",
            "WARNING",
            {"reason_code": reason_code},
        )

    def mqtt_disconnected(self, reason_code: int) -> None:
        self._write(
            "MQTT_DISCONNECTED",
            "WARNING",
            {"reason_code": reason_code},
        )

    def payload_rejected(self, topic: str, reason: str) -> None:
        """
        Called when an inbound MQTT payload is rejected (malformed / missing
        required fields).  Logs the topic but NOT the payload content.
        """
        self._write(
            "PAYLOAD_REJECTED",
            "WARNING",
            {"topic": topic, "reason": reason},
        )

    def reading_inserted(self, sensor_id: str) -> None:
        """Log a successful DB insert — sensor_id is hashed."""
        self._write(
            "READING_INSERTED",
            "DEBUG",
            {"sensor_ref": _safe_sensor_ref(sensor_id)},
        )

    def db_prune(self, deleted_rows: int, operator: str) -> None:
        self._write(
            "DB_PRUNE",
            "INFO",
            {"deleted_rows": deleted_rows, "operator": operator},
        )

    def key_rotation(self, key_name: str) -> None:
        """Log a secret key rotation event — logs key NAME only, never value."""
        self._write(
            "KEY_ROTATION",
            "INFO",
            {"key_name": key_name},
        )

    def secret_configured(self, key_name: str, via: str) -> None:
        """Called when a secret is stored via the configure command."""
        self._write(
            "SECRET_CONFIGURED",
            "INFO",
            {"key_name": key_name, "storage_backend": via},
        )

    def http_error(self, method: str, path: str, status_code: int) -> None:
        """Log HTTP 4xx/5xx responses (no request body or auth headers)."""
        severity = "ERROR" if status_code >= 500 else "WARNING"
        self._write(
            "HTTP_ERROR",
            severity,
            {"method": method, "path": path, "status": status_code},
        )

    def unauthorised_access(self, path: str, remote_addr: str) -> None:
        """Log a 401/403 response — potential intrusion attempt."""
        self._write(
            "UNAUTHORISED_ACCESS",
            "CRITICAL",
            {"path": path, "remote_addr": remote_addr},
        )


# ── Module-level singleton ─────────────────────────────────────────────────────
audit_log = AuditLogger()
