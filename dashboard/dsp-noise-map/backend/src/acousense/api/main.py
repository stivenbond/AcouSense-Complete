import time
from contextlib import asynccontextmanager
from uuid import uuid4

import structlog
from fastapi import Depends, FastAPI, HTTPException, Request, status
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
from slowapi import Limiter, _rate_limit_exceeded_handler
from slowapi.errors import RateLimitExceeded
from slowapi.util import get_remote_address

from acousense.api.routers import items
from acousense.db.connection import get_db
from acousense.utils.logging import configure_logging
from acousense.utils.secrets import get_secret

configure_logging()
logger = structlog.get_logger(__name__)
limiter = Limiter(key_func=get_remote_address, default_limits=["60/minute"])


def check_api_key(request: Request):
    api_key_header = request.headers.get("X-API-Key")
    try:
        valid_key = get_secret("acousense", "api-key")
        if api_key_header == valid_key:
            return
    except Exception:
        pass
    raise HTTPException(
        status_code=status.HTTP_401_UNAUTHORIZED,
        detail="Invalid or missing X-API-Key header",
    )


@asynccontextmanager
async def lifespan(app: FastAPI):
    # Run Alembic migrations on startup
    import os

    from alembic import command
    from alembic.config import Config

    alembic_cfg = Config(os.path.join(os.getcwd(), "alembic.ini"))
    alembic_cfg.set_main_option("script_location", "src/acousense/db/migrations")
    try:
        command.upgrade(alembic_cfg, "head")
        logger.info("Alembic migrations completed successfully.")
    except Exception as e:
        logger.error("Alembic migration failed", error=str(e))

    yield

    logger.info("Shutting down API. Connections will close.")


app = FastAPI(
    title="AcouSense API",
    version="2.0.0",
    description="AcouSense Urban Soundscape Intelligence Platform",
    lifespan=lifespan,
)

app.state.limiter = limiter
app.add_exception_handler(RateLimitExceeded, _rate_limit_exceeded_handler)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost", "https://acousense.local"],  # Explicit origins
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.middleware("http")
async def structlog_middleware(request: Request, call_next):
    request_id = str(uuid4())
    structlog.contextvars.bind_contextvars(request_id=request_id)

    # Restrict body size to 1MB
    if request.headers.get("content-length"):
        if int(request.headers["content-length"]) > 1048576:
            return JSONResponse(
                status_code=413, content={"detail": "Payload Too Large"}
            )

    start_time = time.perf_counter()
    response = await call_next(request)
    process_time = time.perf_counter() - start_time

    logger.info(
        "Request processed",
        path=request.url.path,
        method=request.method,
        status_code=response.status_code,
        elapsed=process_time,
    )

    return response


app.include_router(
    items.router, prefix="/api", tags=["Items"], dependencies=[Depends(check_api_key)]
)


@app.get("/health", tags=["System"])
async def health_check():
    import psutil

    from acousense.db.connection import DB_PATH

    return {
        "status": "ok",
        "version": "2.0.0",
        "db_path": DB_PATH,
        "uptime_s": time.time() - psutil.boot_time(),
    }


@app.get("/ready", tags=["System"])
async def readiness_check(db=Depends(get_db)):
    try:
        async with db.execute("SELECT 1 FROM items") as cur:
            await cur.fetchone()
        return {"status": "ready"}
    except Exception:
        raise HTTPException(
            status_code=503, detail="Database not reachable or migrated"
        )
