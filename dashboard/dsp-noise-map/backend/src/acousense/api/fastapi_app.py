"""
AcouSense — REST API (FastAPI)
=============================
Modern ASGI Fast API application.
Replaces the Flask application.
"""

from __future__ import annotations

import threading
from collections.abc import AsyncGenerator
from contextlib import asynccontextmanager
from typing import Any

from fastapi import FastAPI, HTTPException, Request, Response
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
from pydantic import BaseModel

from src.core.diagnostics import diagnostics
from src.core.engine import engine_cache
from src.core.mqtt_client import start_mqtt_thread
from src.persistence.database import (
    fetch_all_readings,
    fetch_exposure_metrics,
    fetch_latest_per_sensor,
    fetch_stats,
    init_db,
)

try:
    from src.security.audit import audit_log

    _HAS_AUDIT = True
except ImportError:
    _HAS_AUDIT = False


@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncGenerator[None, None]:
    # Startup
    init_db()
    audit_log.server_start(host="0.0.0.0", port=8000)

    # Start MQTT daemon thread
    start_mqtt_thread()

    # Start background computation loop
    from src.cli.main import start_engine_loop

    engine_thread = threading.Thread(
        target=start_engine_loop, daemon=True, name="acousense-engine"
    )
    engine_thread.start()

    # Start resource tracking
    diagnostics.start()

    yield
    # Shutdown
    audit_log.server_stop()


app = FastAPI(
    title="AcouSense API",
    description="AcouSense Urban Soundscape Intelligence Platform",
    version="2.0.0",
    lifespan=lifespan,
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.middleware("http")
async def audit_middleware(request: Request, call_next: Any) -> Response:
    response = await call_next(request)
    if _HAS_AUDIT and response.status_code >= 400:
        audit_log.http_error(
            method=request.method,
            url=str(request.url.path),
            status_code=response.status_code,
        )
    return response


# --- Models ---
class SensorStats(BaseModel):
    total_samples: int
    avg_dbz: float | None
    min_dbz: float | None
    max_dbz: float | None


# --- Endpoints ---


@app.get("/api/health")
def health_check() -> dict[str, str]:
    return {"status": "ok"}


@app.get("/api/readings")
def get_readings() -> JSONResponse:
    readings = fetch_all_readings()
    return JSONResponse(status_code=200, content=readings)


@app.get("/api/latest")
def get_latest() -> JSONResponse:
    readings = fetch_latest_per_sensor()
    return JSONResponse(status_code=200, content=readings)


@app.get("/api/stats")
def get_stats() -> JSONResponse:
    stats = fetch_stats()
    return JSONResponse(status_code=200, content=stats)


@app.get("/api/health/exposure")
def get_exposure() -> JSONResponse:
    metrics = fetch_exposure_metrics()
    return JSONResponse(status_code=200, content=metrics)


@app.get("/api/heatmap")
def get_heatmap() -> JSONResponse:
    grid, m_bytes, _, _, cache_dt = engine_cache.get()

    if m_bytes is None:
        return JSONResponse(
            status_code=202, content={"message": "Heatmap computing, please wait"}
        )

    return JSONResponse(status_code=200, content="Stream heatmap matrix instead.")


@app.get("/api/heatmap/matrix")
def get_heatmap_matrix() -> Response:
    _, m_bytes, _, _, _ = engine_cache.get()
    if m_bytes is None:
        raise HTTPException(status_code=503, detail="Not computed yet")

    headers = {
        "Cache-Control": "public, max-age=10",
        "Access-Control-Expose-Headers": "ETag, Content-Length",
    }
    return Response(
        content=m_bytes, media_type="application/octet-stream", headers=headers
    )


@app.get("/api/heatmap/contours")
def get_heatmap_contours() -> JSONResponse:
    _, _, contours_gj, _, cache_dt = engine_cache.get()
    if contours_gj is None:
        raise HTTPException(status_code=503, detail="Not computed yet")
    import json

    return JSONResponse(status_code=200, content=json.loads(contours_gj))
