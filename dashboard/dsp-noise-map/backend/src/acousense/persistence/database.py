"""
AcouSense — Persistence Layer
==============================
Centralises all SQLite access, enforcing:
  • WAL journaling for safe multi-process concurrency
  • Parameterised queries throughout (SQL injection prevention)
  • Multi-column composite index: (sensor_id, ts) for fast group-by analytics
  • 8 MB page-cache budget
  • secure_delete=ON — deleted pages are physically overwritten with zeros
  • Privacy by design — coordinates generalised, sensor_id AES-256 encrypted
"""

from __future__ import annotations

import math
import os
import sqlite3
from collections.abc import Generator
from contextlib import contextmanager
from datetime import datetime, timedelta
from typing import Any

# Security integrations (lazy import pattern avoids circular deps at startup)
try:
    from src.persistence.privacy import sanitize_reading

    _HAS_PRIVACY = True
except ImportError:
    _HAS_PRIVACY = False

try:
    from src.security.secrets import decrypt_value, encrypt_value

    _HAS_ENCRYPTION = True
except ImportError:
    _HAS_ENCRYPTION = False

try:
    from src.security.audit import audit_log

    _HAS_AUDIT = True
except ImportError:
    _HAS_AUDIT = False

# ─────────────────────────────────────────────────────────────────────────────
# Configuration
# ─────────────────────────────────────────────────────────────────────────────
DB_PATH: str = os.environ.get("DB_PATH", "noise_readings.db")
DATA_MAX_AGE_H: int = int(os.environ.get("DATA_MAX_AGE_H", "24"))


# ─────────────────────────────────────────────────────────────────────────────
# Custom SQLite scalar functions (registered per-connection)
# ─────────────────────────────────────────────────────────────────────────────


def _sqlite_log10(x: float | None) -> float | None:
    """LOG10(x) — returns None for non-positive or null values."""
    return math.log10(x) if x is not None and x > 0 else None


def _sqlite_power(x: float | None, y: float | None) -> float | None:
    """POWER(x, y) — safe wrapper."""
    return math.pow(x, y) if x is not None and y is not None else None


# ─────────────────────────────────────────────────────────────────────────────
# Database initialisation
# ─────────────────────────────────────────────────────────────────────────────


def init_db(db_path: str = DB_PATH) -> None:
    """
    Create the readings table and all required indexes.

    Indexes:
      idx_ts           — fast range scans on ts column
      idx_sensor_id    — fast lookups per sensor
      idx_sensor_ts    — COMPOSITE: powers GROUP BY sensor_id + MAX(ts) queries

    Security:
      • secure_delete=ON applied via _apply_pragmas
      • sensor_id column stores AES-256-Fernet ciphertext when encryption available
    """
    with sqlite3.connect(db_path) as conn:
        _apply_pragmas(conn)
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS readings (
                id        INTEGER PRIMARY KEY AUTOINCREMENT,
                sensor_id TEXT    NOT NULL,
                ts        INTEGER NOT NULL,
                leq_dba   REAL    NOT NULL,
                lat       REAL    NOT NULL,
                lng       REAL    NOT NULL,
                gps_valid INTEGER DEFAULT 0,
                created   TEXT    DEFAULT CURRENT_TIMESTAMP
            )
            """
        )
        # Single-column indexes (backward-compatible)
        conn.execute("CREATE INDEX IF NOT EXISTS idx_ts        ON readings(ts)")
        conn.execute("CREATE INDEX IF NOT EXISTS idx_sensor_id ON readings(sensor_id)")
        # Multi-column composite index — key optimisation for analytics queries
        conn.execute(
            "CREATE INDEX IF NOT EXISTS idx_sensor_ts  ON readings(sensor_id, ts)"
        )
        conn.commit()
    if _HAS_AUDIT:
        audit_log.db_init(db_path)


def _apply_pragmas(conn: sqlite3.Connection) -> None:
    """Apply WAL mode and security PRAGMAs to an open connection."""
    conn.execute("PRAGMA journal_mode=WAL")
    conn.execute("PRAGMA synchronous=NORMAL")
    conn.execute("PRAGMA cache_size=-8000")  # 8 MB page cache
    conn.execute("PRAGMA foreign_keys=ON")
    # SECURITY: overwrite freed pages with zeros — prevents data carving
    # after DELETE operations from recovering sensor readings.
    conn.execute("PRAGMA secure_delete=ON")


# ─────────────────────────────────────────────────────────────────────────────
# Connection context manager
# ─────────────────────────────────────────────────────────────────────────────


@contextmanager
def get_db(db_path: str = DB_PATH) -> Generator[sqlite3.Connection, None, None]:
    """
    Yield a configured SQLite connection with WAL mode and custom scalar
    functions registered.  Commits on success, rolls back on exception.
    """
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    _apply_pragmas(conn)
    conn.create_function("LOG10", 1, _sqlite_log10)
    conn.create_function("POWER", 2, _sqlite_power)
    try:
        yield conn
        conn.commit()
    except Exception:
        conn.rollback()
        raise
    finally:
        conn.close()


# ─────────────────────────────────────────────────────────────────────────────
# Write operations (all parameterised)
# ─────────────────────────────────────────────────────────────────────────────


def insert_reading(payload: dict[str, Any], db_path: str = DB_PATH) -> bool:
    """
    Insert a single sensor reading.  Returns True on success.

    Security pipeline applied before storage:
      1. Field validation — reject payloads missing required fields
      2. Privacy sanitisation — generalise coordinates, add DP noise to dBa
      3. sensor_id encryption — AES-256 Fernet when encryption available

    All values bound via parameterised queries — no string formatting.
    """
    required: set[str] = {"sensor_id", "ts", "leq_dba", "lat", "lng"}
    if not required.issubset(payload.keys()):
        return False

    # Step 1: Privacy transformations (coord generalisation + DP noise)
    if _HAS_PRIVACY:
        payload = sanitize_reading(payload)

    # Step 2: Encrypt sensor_id (most linkable field) before storage
    raw_sensor_id: str = str(payload["sensor_id"])
    stored_sensor_id = (
        encrypt_value(raw_sensor_id) if _HAS_ENCRYPTION else raw_sensor_id
    )

    with get_db(db_path) as conn:
        conn.execute(
            """
            INSERT INTO readings (sensor_id, ts, leq_dba, lat, lng, gps_valid)
            VALUES (?, ?, ?, ?, ?, ?)
            """,
            (
                stored_sensor_id,
                int(payload["ts"]),
                float(payload["leq_dba"]),
                float(payload["lat"]),
                float(payload["lng"]),
                int(payload.get("gps_valid", 0)),
            ),
        )
    if _HAS_AUDIT:
        audit_log.reading_inserted(raw_sensor_id)
    return True


def prune_old_readings(
    max_age_hours: int = DATA_MAX_AGE_H,
    db_path: str = DB_PATH,
    operator: str = "system",
) -> int:
    """
    Delete readings older than *max_age_hours* hours.
    Returns the number of rows deleted.

    With secure_delete=ON, SQLite overwrites freed pages with zeros on commit,
    ensuring deleted sensor readings are unrecoverable from the disk image.
    """
    cutoff_ms = int(
        (datetime.now() - timedelta(hours=max_age_hours)).timestamp() * 1000
    )
    with get_db(db_path) as conn:
        deleted: int = conn.execute(
            "DELETE FROM readings WHERE ts < ?", (cutoff_ms,)
        ).rowcount
    if _HAS_AUDIT:
        audit_log.db_prune(deleted_rows=deleted, operator=operator)
    return deleted


def seed_mock_readings(
    n: int = 20,
    sensor_id: str = "ACU-DEMO",
    center_lat: float = 52.52,
    center_lng: float = 13.405,
    db_path: str = DB_PATH,
) -> int:
    """
    Insert *n* synthetic readings around a centre point.
    Useful for local development without real MQTT hardware.
    """
    import random
    import time

    now_ms = int(time.time() * 1000)
    rows: list[tuple[Any, ...]] = []
    rng = random.Random(42)
    for i in range(n):
        rows.append(
            (
                f"{sensor_id}-{i % 5:02d}",
                now_ms - i * 30_000,  # 30 s apart
                round(rng.uniform(38.0, 78.0), 1),
                center_lat + rng.gauss(0, 0.002),
                center_lng + rng.gauss(0, 0.003),
                1,
            )
        )
    with get_db(db_path) as conn:
        conn.executemany(
            """
            INSERT INTO readings (sensor_id, ts, leq_dba, lat, lng, gps_valid)
            VALUES (?, ?, ?, ?, ?, ?)
            """,
            rows,
        )
    return n


# ─────────────────────────────────────────────────────────────────────────────
# Read operations
# ─────────────────────────────────────────────────────────────────────────────


def fetch_recent_readings(
    minutes: int = 2,
    db_path: str = DB_PATH,
) -> list[dict[str, Any]]:
    """Return per-sensor aggregated readings from the last *minutes* minutes."""
    since_ms = int((datetime.now() - timedelta(minutes=minutes)).timestamp() * 1000)
    with get_db(db_path) as conn:
        rows = conn.execute(
            """
            SELECT
                sensor_id,
                AVG(lat)                                          AS lat,
                AVG(lng)                                          AS lng,
                10 * LOG10(AVG(POWER(10, leq_dba / 10.0)))       AS leq_dba,
                MAX(ts)                                           AS ts
            FROM readings
            WHERE ts >= ?
            GROUP BY sensor_id
            """,
            (since_ms,),
        ).fetchall()
    return [dict(r) for r in rows if r["leq_dba"] is not None]


def fetch_all_readings(
    minutes: int = 60,
    sensor_id: str | None = None,
    db_path: str = DB_PATH,
) -> list[dict[str, Any]]:
    """Return raw readings with optional sensor_id filter."""
    since_ms = int((datetime.now() - timedelta(minutes=minutes)).timestamp() * 1000)
    params: list[Any] = [since_ms]
    query = "SELECT * FROM readings WHERE ts >= ?"
    if sensor_id:
        query += " AND sensor_id = ?"
        params.append(sensor_id)
    query += " ORDER BY ts DESC"
    with get_db(db_path) as conn:
        rows = conn.execute(query, params).fetchall()
    return [dict(r) for r in rows]


def fetch_latest_per_sensor(db_path: str = DB_PATH) -> list[dict[str, Any]]:
    """Return the single most-recent reading for each sensor."""
    with get_db(db_path) as conn:
        rows = conn.execute(
            """
            SELECT r1.*
            FROM readings r1
            WHERE r1.id = (
                SELECT MAX(r2.id) FROM readings r2
                WHERE r2.sensor_id = r1.sensor_id
            )
            """
        ).fetchall()
    return [dict(r) for r in rows]


def fetch_stats(db_path: str = DB_PATH) -> dict[str, Any]:
    """Return aggregate statistics across all readings."""
    with get_db(db_path) as conn:
        sensor_count: int = conn.execute(
            "SELECT COUNT(DISTINCT sensor_id) FROM readings"
        ).fetchone()[0]
        reading_count: int = conn.execute("SELECT COUNT(*) FROM readings").fetchone()[0]
        row = conn.execute(
            """
            SELECT
                MIN(leq_dba)                                        AS min_dba,
                MAX(leq_dba)                                        AS max_dba,
                10 * LOG10(AVG(POWER(10, leq_dba / 10.0)))          AS avg_dba
            FROM readings
            """
        ).fetchone()
    return {
        "sensor_count": sensor_count,
        "reading_count": reading_count,
        "min_dba": round(row["min_dba"], 1) if row["min_dba"] is not None else 0.0,
        "max_dba": round(row["max_dba"], 1) if row["max_dba"] is not None else 0.0,
        "avg_dba": round(row["avg_dba"], 1) if row["avg_dba"] is not None else 0.0,
    }


def fetch_exposure_metrics(
    days: int = 7,
    db_path: str = DB_PATH,
) -> dict[str, Any]:
    """
    Compute WHO exposure classification bucketed by minute.
    Returns green / yellow / red minute counts and average dBa.
    """
    since_ms = int((datetime.now() - timedelta(days=days)).timestamp() * 1000)
    with get_db(db_path) as conn:
        minute_rows = conn.execute(
            """
            SELECT (ts / 60000) AS minute_bucket, MAX(leq_dba) AS max_dba
            FROM readings
            WHERE ts >= ?
            GROUP BY minute_bucket
            """,
            (since_ms,),
        ).fetchall()
        avg_row = conn.execute(
            """
            SELECT 10 * LOG10(AVG(POWER(10, leq_dba / 10.0))) AS true_avg
            FROM readings WHERE ts >= ?
            """,
            (since_ms,),
        ).fetchone()

    green = yellow = red = 0
    for row in minute_rows:
        if row["max_dba"] < 65:
            green += 1
        elif row["max_dba"] < 85:
            yellow += 1
        else:
            red += 1

    avg_dba = avg_row["true_avg"] if avg_row["true_avg"] is not None else 0.0
    return {
        "days": days,
        "metrics": {
            "green_minutes": green,
            "yellow_minutes": yellow,
            "red_minutes": red,
            "total_evaluated_minutes": len(minute_rows),
            "average_dba": round(avg_dba, 1),
        },
    }
