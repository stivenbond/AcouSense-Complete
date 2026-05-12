"""
AcouSense — Privacy by Design
================================
Geospatial privacy utilities enforcing data minimisation and anonymisation.

Principles implemented
----------------------
1. **Coordinate Generalisation** (k-anonymity approach)
   Exact GPS coordinates are quantised to 100-metre grid cell centroids
   before storage.  This ensures multiple sensors in close proximity all
   map to the same cell centroid, preventing reverse-geocoding of individual
   device locations while preserving neighbourhood-level spatial resolution
   needed for acoustic heatmaps.

2. **Differential Privacy noise** (Laplace mechanism)
   A calibrated Laplace-distributed noise term is added to aggregate dbz
   values returned by the API.  With ε = 0.5 and sensitivity = 1 dB,
   individual readings cannot be inferred from the published aggregate,
   while the urban noise map retains statistical utility at the
   neighbourhood scale (noise ≈ ±2 dB — within measurement uncertainty).

References
----------
- Dwork & Roth, "The Algorithmic Foundations of Differential Privacy" (2014)
- ISO 19157 Geographic Information — Data Quality
- GDPR Article 25 — Data Protection by Design and by Default
"""

from __future__ import annotations

import math
import secrets

# ─────────────────────────────────────────────────────────────────────────────
# Coordinate generalisation
# ─────────────────────────────────────────────────────────────────────────────

# Earth radius used for coordinate arithmetic
_R_EARTH_M: float = 6_371_000.0


def generalize_coords(
    lat: float,
    lng: float,
    cell_m: float = 100.0,
) -> tuple[float, float]:
    """
    Snap *lat* / *lng* to the centroid of a ``cell_m``-metre grid cell.

    Algorithm
    ---------
    1. Convert cell_m to degrees (approximation valid at mid-latitudes).
    2. Quantise each coordinate to the nearest cell boundary.
    3. Return the cell centroid (boundary + half-cell offset).

    The default cell size of 100 m produces ~6 decimal place precision,
    which is insufficient to identify an individual building but sufficient
    for neighbourhood-level acoustic mapping.

    Args:
        lat:    Latitude in decimal degrees.
        lng:    Longitude in decimal degrees.
        cell_m: Grid cell edge length in metres. Default 100 m.

    Returns:
        (generalised_lat, generalised_lng) tuple rounded to 6 d.p.
    """
    # Degrees per metre at this latitude
    lat_deg_per_m = 1.0 / (_R_EARTH_M * math.pi / 180.0)
    lng_deg_per_m = 1.0 / (_R_EARTH_M * math.cos(math.radians(lat)) * math.pi / 180.0)

    cell_lat = cell_m * lat_deg_per_m
    cell_lng = cell_m * lng_deg_per_m

    # Snap to nearest grid boundary then shift to centroid
    gen_lat = math.floor(lat / cell_lat) * cell_lat + cell_lat / 2.0
    gen_lng = math.floor(lng / cell_lng) * cell_lng + cell_lng / 2.0

    return round(gen_lat, 6), round(gen_lng, 6)


# ─────────────────────────────────────────────────────────────────────────────
# Differential Privacy — Laplace mechanism
# ─────────────────────────────────────────────────────────────────────────────


def _laplace_noise(sensitivity: float, epsilon: float) -> float:
    """
    Generate a single sample from Laplace(0, sensitivity/epsilon) using the
    inverse CDF method.  Uses ``secrets.SystemRandom`` for a CSPRNG source.

    Args:
        sensitivity: Global sensitivity of the query (Δf).
        epsilon:     Privacy budget (ε).  Smaller = more privacy.

    Returns:
        A float noise value drawn from Lap(0, Δf/ε).
    """
    scale = sensitivity / epsilon
    rng = secrets.SystemRandom()
    u = rng.random() - 0.5
    # Inverse CDF: sign(u) * ln(1 - 2|u|) * (-scale)
    return -scale * math.copysign(1.0, u) * math.log(1.0 - 2.0 * abs(u))


def add_dp_noise(
    dbz_value: float,
    epsilon: float = 0.5,
    sensitivity: float = 1.0,
) -> float:
    """
    Apply the Laplace differential privacy mechanism to a dbz measurement.

    With the default parameters (ε=0.5, Δf=1 dB) the expected noise magnitude
    is ±2 dB — within the ±3 dB measurement uncertainty of consumer MEMS
    microphones, so the map retains full practical utility.

    Args:
        dbz_value:   The aggregate sound level in dB(Z).
        epsilon:     Privacy budget.  Default 0.5 (moderate privacy).
        sensitivity: The maximum influence of a single reading on the
                     aggregated output.  Default 1.0 dB.

    Returns:
        dbz_value + Laplace noise, clamped to [0.0, 140.0] dB.
    """
    noisy = dbz_value + _laplace_noise(sensitivity, epsilon)
    return round(max(0.0, min(140.0, noisy)), 1)


# ─────────────────────────────────────────────────────────────────────────────
# Convenience: privacy-safe payload preprocessing
# ─────────────────────────────────────────────────────────────────────────────


def sanitize_reading(
    payload: dict,
    coord_cell_m: float = 100.0,
    dp_epsilon: float = 0.5,
) -> dict:
    """
    Apply all privacy transformations to an inbound sensor reading dict
    before it is written to the database.

    Transforms applied:
      • Coordinate generalisation (lat/lng snapped to 100 m grid)
      • dbz differential privacy noise

    The ``sensor_id`` is encrypted at the database layer via
    ``src.security.secrets.encrypt_value`` — not handled here.

    Args:
        payload:      Raw MQTT payload dict with lat, lng, leq_dbz keys.
        coord_cell_m: Grid cell size for coordinate generalisation.
        dp_epsilon:   Differential privacy budget.

    Returns:
        Modified copy of *payload* with privacy transformations applied.
    """
    out = dict(payload)

    if "lat" in out and "lng" in out:
        out["lat"], out["lng"] = generalize_coords(
            float(out["lat"]), float(out["lng"]), cell_m=coord_cell_m
        )

    if "leq_dbz" in out:
        out["leq_dbz"] = add_dp_noise(float(out["leq_dbz"]), epsilon=dp_epsilon)

    return out
