"""
AcouSense — REST API (Flask)
=============================
Exposes the core computational engine and SQLite persistence layer via HTTP.
Contains caching middleware (ETags and cache-control properties).
Runs in its own synchronous thread, while the Engine and MQTT loops run in the background.

Security:
  • Audit logging on all HTTP 4xx/5xx responses via after_request hook
  • No request bodies or auth headers are logged — only method + path + status
"""

from __future__ import annotations

import io
from datetime import datetime
from typing import Any

import mercantile
import numpy as np
from flask import Flask, Response, jsonify, request
from flask_cors import CORS
from matplotlib.colors import LinearSegmentedColormap
from PIL import Image

from acousense.core.engine import engine_cache
from acousense.persistence.database import (
    fetch_all_readings,
    fetch_exposure_metrics,
    fetch_latest_per_sensor,
    fetch_stats,
)

# ── Security: audit logger ──────────────────────────────────────────────────
try:
    from acousense.security.audit import audit_log

    _HAS_AUDIT = True
except ImportError:
    _HAS_AUDIT = False

# ── Flask Setup ────────────────────────────────────────────────────────────
app = Flask(__name__)
CORS(app)


@app.after_request
def _audit_http_errors(response: Response) -> Response:
    """Log all 4xx/5xx HTTP responses to the security audit trail."""
    if _HAS_AUDIT and response.status_code >= 400:
        audit_log.http_error(
            method=request.method,
            path=request.path,
            status_code=response.status_code,
        )
    return response


DB_RANGE_MIN = 25.0
DB_RANGE_MAX = 95.0

# ── WHO 7-stop perceptually-uniform colour scale ───────────────────────────
WHO_STOPS: list[tuple[float, str]] = [
    (0.00, "#1a237e"),  # < 35 dB  — deep blue  (safe)
    (0.10, "#00897b"),  # 35 dB    — teal
    (0.25, "#43a047"),  # 45 dB    — green
    (0.40, "#fdd835"),  # 55 dB    — yellow
    (0.55, "#fb8c00"),  # 65 dB    — orange
    (0.75, "#e53935"),  # 75 dB    — red
    (1.00, "#b71c1c"),  # 85+ dB   — deep red   (danger)
]


def _build_cmap() -> LinearSegmentedColormap:
    positions = [s[0] for s in WHO_STOPS]
    colors = [s[1] for s in WHO_STOPS]
    return LinearSegmentedColormap.from_list(
        "who_noise", list(zip(positions, colors)), N=256
    )


WHO_CMAP = _build_cmap()


def db_to_rgba(db_grid: np.ndarray) -> np.ndarray:
    """Convert dB grid → (H, W, 4) uint8 RGBA image array."""
    norm = (db_grid - DB_RANGE_MIN) / (DB_RANGE_MAX - DB_RANGE_MIN)
    norm = np.clip(norm, 0, 1)
    rgba = WHO_CMAP(norm)
    return (rgba * 255).astype(np.uint8)


# ── HTTP Helpers ───────────────────────────────────────────────────────────


def add_cache_headers(
    resp: Response, etag: str | None = None, max_age: int = 5
) -> Response:
    resp.headers["Cache-Control"] = f"public, max-age={max_age}"
    if etag:
        resp.headers["ETag"] = f'"{etag}"'
    return resp


def check_etag(target_etag: str) -> bool:
    """Return True if client etag matches, supporting 304 Not Modified."""
    client_etag = request.headers.get("If-None-Match", "").strip('"')
    if client_etag and client_etag == target_etag:
        return True
    return False


# ═══════════════════════════════════════════════════════════════════════════════
# Base API Endpoints (Database reads)
# ═══════════════════════════════════════════════════════════════════════════════


@app.route("/api/readings", methods=["GET"])
def api_readings() -> Response:
    minutes = request.args.get("minutes", default=60, type=int)
    sensor_id = request.args.get("sensor_id")
    rows = fetch_all_readings(minutes=minutes, sensor_id=sensor_id)
    resp = jsonify(rows)
    return add_cache_headers(resp, max_age=3)


@app.route("/api/latest", methods=["GET"])
def api_latest() -> Response:
    rows = fetch_latest_per_sensor()
    resp = jsonify(rows)
    return add_cache_headers(resp, max_age=2)


@app.route("/api/stats", methods=["GET"])
def api_stats() -> Response:
    stats = fetch_stats()
    resp = jsonify(stats)
    return add_cache_headers(resp, max_age=5)


@app.route("/api/health/exposure", methods=["GET"])
def api_health_exposure() -> Response:
    days = request.args.get("days", default=7, type=int)
    metrics = fetch_exposure_metrics(days=days)
    resp = jsonify(metrics)
    return add_cache_headers(resp, max_age=60)


# ═══════════════════════════════════════════════════════════════════════════════
# Engine Products (Heatmap)
# ═══════════════════════════════════════════════════════════════════════════════


def _get_engine_snapshot() -> tuple[dict[str, Any] | None, str, str | None]:
    return engine_cache.snapshot()


@app.route("/api/heatmap", methods=["GET"])
def api_heatmap() -> Response:
    result, etag, _ = _get_engine_snapshot()
    if result is None:
        return jsonify(
            {
                "readings": [],
                "grid": [],
                "bbox": {"min_lat": 0, "min_lng": 0, "max_lat": 0, "max_lng": 0},
                "generated_at": datetime.now().isoformat(),
                "error": "Engine not ready",
            }
        )
    if check_etag(etag):
        return Response(status=304)

    bbox = result["bbox"]
    db_grid = result["grid_db"]
    res = result["resolution"]

    # Downsample to a JSON-friendly resolution (~50x50)
    step = max(1, res // 50)
    grid_pts = []
    lats = np.linspace(bbox[0], bbox[2], res)
    lons = np.linspace(bbox[1], bbox[3], res)
    for i in range(0, res, step):
        for j in range(0, res, step):
            grid_pts.append(
                {
                    "lat": round(float(lats[i]), 6),
                    "lng": round(float(lons[j]), 6),
                    "leq_dba": round(float(db_grid[i, j]), 2),
                }
            )

    payload = {
        "readings": [],
        "grid": grid_pts,
        "bbox": {
            "min_lat": bbox[0],
            "min_lng": bbox[1],
            "max_lat": bbox[2],
            "max_lng": bbox[3],
        },
        "generated_at": datetime.now().isoformat(),
    }
    resp = jsonify(payload)
    return add_cache_headers(resp, etag=etag, max_age=5)


@app.route("/api/heatmap/matrix", methods=["GET"])
def api_heatmap_matrix() -> Response:
    result, etag, _ = _get_engine_snapshot()
    if result is None:
        return Response("Engine not ready", status=503)
    if check_etag(etag):
        return Response(status=304)

    db_grid = result["grid_db"].astype(np.float32)
    body = db_grid.tobytes()
    bbox = result["bbox"]

    resp = Response(body, mimetype="application/octet-stream")
    resp.headers["X-Grid-Rows"] = str(result["resolution"])
    resp.headers["X-Grid-Cols"] = str(result["resolution"])
    resp.headers["X-Min-Lat"] = str(bbox[0])
    resp.headers["X-Min-Lon"] = str(bbox[1])
    resp.headers["X-Max-Lat"] = str(bbox[2])
    resp.headers["X-Max-Lon"] = str(bbox[3])
    resp.headers["X-Min-DB"] = str(result["min_db"])
    resp.headers["X-Max-DB"] = str(result["max_db"])
    return add_cache_headers(resp, etag=etag, max_age=5)


@app.route("/api/heatmap/contours", methods=["GET"])
def api_heatmap_contours() -> Response:
    _, etag, gj = _get_engine_snapshot()
    if gj is None:
        return jsonify({"type": "FeatureCollection", "features": []})
    if check_etag(etag):
        return Response(status=304)

    resp = Response(gj, mimetype="application/geo+json")
    return add_cache_headers(resp, etag=etag, max_age=5)


@app.route("/api/heatmap/tiles/<int:z>/<int:x>/<int:y>.png", methods=["GET"])
def api_heatmap_tile(z: int, x: int, y: int) -> Response:
    result, etag, _ = _get_engine_snapshot()

    def transparent_tile() -> Response:
        img = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
        buf = io.BytesIO()
        img.save(buf, format="PNG")
        return Response(buf.getvalue(), mimetype="image/png")

    if result is None:
        return transparent_tile()

    tile_bounds = mercantile.bounds(x, y, z)
    bbox = result["bbox"]
    db_grid = result["grid_db"]
    res = result["resolution"]

    # Map tile bounds into grid indices
    lat_range = bbox[2] - bbox[0]
    lon_range = bbox[3] - bbox[1]

    if lat_range == 0 or lon_range == 0:
        return transparent_tile()

    # Fractional pixel indices for the tile's geo extent
    col_start = (tile_bounds.west - bbox[1]) / lon_range * res
    col_end = (tile_bounds.east - bbox[1]) / lon_range * res
    row_start = (1.0 - (tile_bounds.north - bbox[0]) / lat_range) * res
    row_end = (1.0 - (tile_bounds.south - bbox[0]) / lat_range) * res

    # Clamp
    r0 = max(0, int(row_start))
    r1 = min(res, int(row_end) + 1)
    c0 = max(0, int(col_start))
    c1 = min(res, int(col_end) + 1)

    if r1 <= r0 or c1 <= c0:
        return transparent_tile()

    sub = db_grid[r0:r1, c0:c1]
    rgba = db_to_rgba(sub)
    img = Image.fromarray(rgba, "RGBA").resize((256, 256), Image.BILINEAR)

    buf = io.BytesIO()
    img.save(buf, format="PNG", optimize=True)
    resp = Response(buf.getvalue(), mimetype="image/png")
    return add_cache_headers(resp, etag=etag, max_age=10)


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)
