"""
AcouSense — Core Computation Engine
=====================================
Separates all geospatial, acoustics, and interpolation logic from the
HTTP layer.  Exposes a high-level `generate_realtime_heatmap` function
and a thread-safe `EngineCache` singleton.

Pipeline:
  1. Ingest sensor readings  →  Cartesian sensor points with temporal weights
  2. Physical propagation    →  ISO 9613-2 spreading + atmospheric absorption
  3. Adaptive refinement     →  RBF thin-plate-spline or Ordinary Kriging
  4. Sensor-correction pass  →  residual RBF to honour ground-truth values
  5. Gaussian smoothing      →  final aesthetic smoothing pass
"""

from __future__ import annotations

import hashlib
import math
import os
import threading
import time
from dataclasses import dataclass, field
from typing import Any

import numpy as np
from scipy.interpolate import RBFInterpolator, RegularGridInterpolator
from scipy.ndimage import gaussian_filter

# ── Optional heavy deps ────────────────────────────────────────────────────
try:
    from pykrige.ok import OrdinaryKriging  # type: ignore[import]

    HAS_PYKRIGE = True
except ImportError:
    HAS_PYKRIGE = False

# ─────────────────────────────────────────────────────────────────────────────
# Physical constants  (ISO 1683 / ISO 9613-2)
# ─────────────────────────────────────────────────────────────────────────────
P_REF: float = 20e-6  # reference sound pressure, Pa
C_SOUND: float = 343.0  # m/s at 20 °C
RHO_AIR: float = 1.204  # kg/m³ at 20 °C, 1 atm
R_EARTH: int = 6_371_000  # metres

# ─────────────────────────────────────────────────────────────────────────────
# Grid / engine configuration
# ─────────────────────────────────────────────────────────────────────────────
GRID_RESOLUTION: int = int(os.environ.get("GRID_RESOLUTION", "512"))
ENGINE_INTERVAL: float = float(os.environ.get("ENGINE_INTERVAL", "5.0"))


# ═══════════════════════════════════════════════════════════════════════════════
# Coordinate helpers
# ═══════════════════════════════════════════════════════════════════════════════


def latlon_to_cartesian(
    lat: float, lon: float, origin_lat: float, origin_lon: float
) -> tuple[float, float]:
    """Project geographic coords to metres relative to an origin point."""
    x = R_EARTH * math.radians(lon - origin_lon) * math.cos(math.radians(origin_lat))
    y = R_EARTH * math.radians(lat - origin_lat)
    return x, y


def build_cartesian_grid(
    bbox: tuple[float, float, float, float],
    resolution: int,
) -> tuple[np.ndarray, np.ndarray, float, float, np.ndarray, np.ndarray]:
    """
    Build a 2-D Cartesian mesh grid in metres spanning *bbox*.

    Returns:
        X, Y            — 2-D arrays of x/y coordinates
        origin_lat/lon  — projection origin (bbox centre)
        xs, ys          — 1-D edge arrays
    """
    min_lat, min_lon, max_lat, max_lon = bbox
    origin_lat = (min_lat + max_lat) / 2.0
    origin_lon = (min_lon + max_lon) / 2.0

    x0, _ = latlon_to_cartesian(min_lat, min_lon, origin_lat, origin_lon)
    x1, _ = latlon_to_cartesian(min_lat, max_lon, origin_lat, origin_lon)
    _, y0 = latlon_to_cartesian(min_lat, min_lon, origin_lat, origin_lon)
    _, y1 = latlon_to_cartesian(max_lat, min_lon, origin_lat, origin_lon)

    xs = np.linspace(x0, x1, resolution)
    ys = np.linspace(y0, y1, resolution)
    X, Y = np.meshgrid(xs, ys)
    return X, Y, origin_lat, origin_lon, xs, ys


def haversine_km(lat1: float, lng1: float, lat2: float, lng2: float) -> float:
    """Great-circle distance between two lat/lng points in kilometres."""
    R = 6371.0
    d_lat = math.radians(lat2 - lat1)
    d_lng = math.radians(lng2 - lng1)
    a = math.sin(d_lat / 2) ** 2 + (
        math.cos(math.radians(lat1))
        * math.cos(math.radians(lat2))
        * math.sin(d_lng / 2) ** 2
    )
    return R * 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))


# ═══════════════════════════════════════════════════════════════════════════════
# Satellite Morphology Layer
# ═══════════════════════════════════════════════════════════════════════════════


class MorphologyLayer:
    """
    Encapsulates satellite-derived urban morphology for ISO 9613-2 propagation.

    In production, replace the ``mock=True`` branch with rasterio ingestion of
    Sentinel-2 / SRTM GeoTIFF bands to populate real building heights and
    ground-cover absorption coefficients.
    """

    ABSORPTION: dict[str, float] = {
        "asphalt": 0.02,
        "grass": 0.35,
        "water": 0.01,
        "bare_soil": 0.15,
        "concrete": 0.03,
    }

    def __init__(self, grid_shape: tuple[int, int], mock: bool = True) -> None:
        H, W = grid_shape
        if mock:
            rng = np.random.default_rng(42)
            self.building_density: np.ndarray = rng.uniform(0.1, 0.9, (H, W))
            self.avg_building_h: np.ndarray = rng.uniform(3.0, 40.0, (H, W))
            self.elevation: np.ndarray = gaussian_filter(
                rng.uniform(0, 30, (H, W)), sigma=20
            )
            self.surface_alpha: np.ndarray = np.full((H, W), 0.02)
        else:
            raise NotImplementedError(
                "Provide GeoTIFF rasterio paths for production morphology."
            )

    def get_insertion_loss(self) -> np.ndarray:
        """Urban building insertion loss in dB (clamped 0–25 dB)."""
        il = 10 * np.log10(1 + (self.building_density * self.avg_building_h / 5.0))
        return np.clip(il, 0, 25)

    def get_ground_absorption(self) -> np.ndarray:
        """Ground absorption coefficient α (0–1)."""
        return self.surface_alpha


# ═══════════════════════════════════════════════════════════════════════════════
# Physical propagation  (ISO 9613-2)
# ═══════════════════════════════════════════════════════════════════════════════


def ingest_sensor_readings(
    raw_readings: list[dict[str, Any]],
    origin_lat: float,
    origin_lon: float,
) -> np.ndarray:
    """
    Convert raw reading dicts → ndarray of shape (N, 3) [x_m, y_m, power_Pa²]
    applying temporal weighting: readings < 30 s old get 2× weight.
    Stale readings > 120 s are discarded entirely.
    """
    now = time.time()
    pts: list[list[float]] = []
    for r in raw_readings:
        ts_s = r["ts"] / 1000.0 if r["ts"] > 1e12 else r["ts"]
        age = now - ts_s
        if age > 120:
            continue
        weight = 2.0 if age < 30 else 1.0
        x, y = latlon_to_cartesian(r["lat"], r["lng"], origin_lat, origin_lon)
        power = (10.0 ** (r["leq_dba"] / 10.0)) * (P_REF**2) * weight
        pts.append([x, y, power])
    return np.array(pts) if pts else np.zeros((0, 3))


def atmospheric_absorption(
    distance_m: np.ndarray,
    freq_hz: float = 1000.0,
    temp_c: float = 20.0,
    humidity: float = 50.0,
) -> np.ndarray:
    """Simple atmospheric absorption (dB) as a function of distance."""
    alpha = 0.005 * (1 + 0.02 * (temp_c - 20)) * (humidity / 50.0)
    return alpha * distance_m


def propagate_sources(
    sensor_pts: np.ndarray,
    grid_X: np.ndarray,
    grid_Y: np.ndarray,
    morphology: MorphologyLayer,
) -> np.ndarray:
    """
    Full ISO 9613-2 propagation from all sensor sources onto the grid.

    Accounts for:
      • Geometrical spherical spreading (1 / 4πr²)
      • Atmospheric absorption
      • Urban building insertion loss
      • Ground absorption coefficient
    """
    H, W = grid_X.shape
    total_power = np.zeros((H, W))
    if sensor_pts.shape[0] == 0:
        return total_power

    insertion_loss_db = morphology.get_insertion_loss()
    ground_alpha = morphology.get_ground_absorption()
    barrier_lin = 10.0 ** (-insertion_loss_db / 10.0)
    ground_lin = 1.0 - ground_alpha

    for sx, sy, s_power in sensor_pts:
        r = np.sqrt((grid_X - sx) ** 2 + (grid_Y - sy) ** 2) + 1e-6
        spreading = 1.0 / (4.0 * np.pi * r**2)
        atm_db = atmospheric_absorption(r)
        atm_lin = 10.0 ** (-atm_db / 10.0)
        cell_power = s_power * spreading * atm_lin * barrier_lin * ground_lin
        total_power += cell_power

    with np.errstate(divide="ignore"):
        db_grid = 10.0 * np.log10(total_power / (P_REF**2))
    db_grid = np.nan_to_num(db_grid, nan=0.0, neginf=0.0)
    return np.clip(db_grid, 0, 140)


# ═══════════════════════════════════════════════════════════════════════════════
# Adaptive interpolation  (RBF / Kriging)
# ═══════════════════════════════════════════════════════════════════════════════


def _rbf_interpolate(
    xy: np.ndarray,
    values: np.ndarray,
    flat_pts: np.ndarray,
    shape: tuple[int, int],
) -> np.ndarray:
    """Thin-plate-spline RBF interpolation."""
    rbf = RBFInterpolator(xy, values, kernel="thin_plate_spline")
    return rbf(flat_pts).reshape(shape)


def adaptive_interpolate(
    sensor_xy: np.ndarray,
    sensor_db: np.ndarray,
    grid_X: np.ndarray,
    grid_Y: np.ndarray,
) -> np.ndarray:
    """
    Select interpolation strategy based on sensor count:
      • n < 5  → thin-plate-spline RBF (always works)
      • n ≥ 5  → Ordinary Kriging (spherical variogram) if pykrige installed,
                  else falls back to RBF.

    Interpolation is performed in the linear power domain for physical
    correctness rather than in the compressed dB domain.

    Returns a full-grid dB array.
    """
    n = sensor_xy.shape[0]
    if n == 0:
        return np.zeros(grid_X.shape)

    power = (10.0 ** (sensor_db / 10.0)) * (P_REF**2)
    flat_pts = np.column_stack([grid_X.ravel(), grid_Y.ravel()])

    if n >= 5 and HAS_PYKRIGE:
        try:
            ok = OrdinaryKriging(
                sensor_xy[:, 0],
                sensor_xy[:, 1],
                power,
                variogram_model="spherical",
                verbose=False,
                enable_plotting=False,
            )
            z, _ = ok.execute("points", flat_pts[:, 0], flat_pts[:, 1])
            grid_power: np.ndarray = np.array(z).reshape(grid_X.shape)
        except Exception:
            # Kriging can fail at degenerate geometries — graceful fallback
            grid_power = _rbf_interpolate(sensor_xy, power, flat_pts, grid_X.shape)
    else:
        grid_power = _rbf_interpolate(sensor_xy, power, flat_pts, grid_X.shape)

    grid_power = np.maximum(grid_power, 1e-30)
    with np.errstate(divide="ignore"):
        db_grid = 10.0 * np.log10(grid_power / (P_REF**2))
    return np.nan_to_num(db_grid, nan=0.0, neginf=0.0)


def sensor_correction_pass(
    db_grid: np.ndarray,
    sensor_xy: np.ndarray,
    sensor_db: np.ndarray,
    grid_X: np.ndarray,
    grid_Y: np.ndarray,
    xs: np.ndarray,
    ys: np.ndarray,
) -> np.ndarray:
    """
    Residual correction: compute the difference between model-predicted values
    at sensor locations and the measured ground-truth, then spread that residual
    smoothly across the grid via RBF.  Requires ≥ 3 sensors.
    """
    if sensor_xy.shape[0] < 3:
        return db_grid
    interp_fn = RegularGridInterpolator(
        (ys, xs), db_grid, method="linear", bounds_error=False, fill_value=0.0
    )
    predicted = interp_fn(sensor_xy[:, [1, 0]])
    residuals = sensor_db - predicted
    rbf = RBFInterpolator(sensor_xy, residuals, kernel="thin_plate_spline")
    flat_pts = np.column_stack([grid_X.ravel(), grid_Y.ravel()])
    res_field = rbf(flat_pts).reshape(grid_X.shape)
    res_field = gaussian_filter(res_field, sigma=3)
    return np.clip(db_grid + res_field, 0, 140)


# ═══════════════════════════════════════════════════════════════════════════════
# Master heatmap pipeline
# ═══════════════════════════════════════════════════════════════════════════════

_morphology_cache: MorphologyLayer | None = None


def generate_realtime_heatmap(
    readings_raw: list[dict[str, Any]],
    bbox: tuple[float, float, float, float],
) -> dict[str, Any]:
    """
    Master pipeline entrypoint.  Runs the full 5-step computation.

    Args:
        readings_raw:  List of per-sensor aggregated reading dicts.
        bbox:          (min_lat, min_lon, max_lat, max_lon)

    Returns a dict containing the full dB grid and all metadata needed
    by the HTTP layer to render tiles, matrices, and GeoJSON contours.
    """
    global _morphology_cache
    res = GRID_RESOLUTION

    X, Y, olat, olon, xs, ys = build_cartesian_grid(bbox, res)

    # Lazily (re)build morphology if resolution changed
    if _morphology_cache is None or _morphology_cache.building_density.shape != (
        res,
        res,
    ):
        _morphology_cache = MorphologyLayer((res, res), mock=True)
    morphology = _morphology_cache

    # Step 1 — Ingest + temporal weighting
    sensor_pts = ingest_sensor_readings(readings_raw, olat, olon)

    # Step 2 — Physical propagation (ISO 9613-2)
    db_grid = propagate_sources(sensor_pts, X, Y, morphology)

    # Step 3 — Adaptive RBF/Kriging refinement (last 2 min readings only)
    now = time.time()
    recent = [
        r
        for r in readings_raw
        if (now - (r["ts"] / 1000.0 if r["ts"] > 1e12 else r["ts"])) < 120
    ]
    if recent:
        sensor_xy = np.array(
            [latlon_to_cartesian(r["lat"], r["lng"], olat, olon) for r in recent]
        )
        sensor_db = np.array([r["leq_dba"] for r in recent])
        interp_grid = adaptive_interpolate(sensor_xy, sensor_db, X, Y)
        # 60% physical + 40% statistical blend
        db_grid = 0.6 * db_grid + 0.4 * interp_grid

    # Step 4 — Sensor-truth correction
    if len(recent) >= 3:
        sensor_xy = np.array(
            [latlon_to_cartesian(r["lat"], r["lng"], olat, olon) for r in recent]
        )
        sensor_db = np.array([r["leq_dba"] for r in recent])
        db_grid = sensor_correction_pass(db_grid, sensor_xy, sensor_db, X, Y, xs, ys)

    # Step 5 — Final Gaussian smoothing
    db_grid = gaussian_filter(db_grid, sigma=1.5)
    db_grid = np.clip(db_grid, 0, 140)

    return {
        "grid_db": db_grid,
        "min_db": float(np.min(db_grid)),
        "max_db": float(np.max(db_grid)),
        "timestamp": time.time(),
        "bbox": bbox,
        "resolution": res,
        "origin_lat": olat,
        "origin_lon": olon,
    }


# ═══════════════════════════════════════════════════════════════════════════════
# Thread-safe engine cache
# ═══════════════════════════════════════════════════════════════════════════════


@dataclass
class EngineCache:
    """
    Thread-safe shared state between the background engine thread and the
    Flask HTTP handlers.  Prevents blocking the API while computation runs.
    """

    lock: threading.Lock = field(default_factory=threading.Lock)
    result: dict[str, Any] | None = None
    etag: str = ""
    contour_geojson: str | None = None

    def update(
        self,
        result: dict[str, Any],
        contour_geojson: str | None,
    ) -> None:
        etag = hashlib.md5(result["grid_db"].tobytes()[:4096]).hexdigest()
        with self.lock:
            self.result = result
            self.etag = etag
            self.contour_geojson = contour_geojson

    def snapshot(self) -> tuple[dict[str, Any] | None, str, str | None]:
        with self.lock:
            return self.result, self.etag, self.contour_geojson


# Module-level singleton used by both engine loop and HTTP handlers
engine_cache = EngineCache()
