# Stage 1: Builder
FROM python:3.12-slim AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    libsqlcipher-dev \
    && rm -rf /var/lib/apt/lists/*

# Install uv
COPY --from=ghcr.io/astral-sh/uv:latest /uv /uvx /bin/

WORKDIR /app
COPY pyproject.toml .

RUN uv venv
ENV VIRTUAL_ENV=/app/.venv
ENV PATH="$VIRTUAL_ENV/bin:$PATH"

# User requirements explicitly state compiling sqlcipher3 wheel here
RUN CFLAGS="-I/usr/include/sqlcipher" LDFLAGS="-L/usr/lib/x86_64-linux-gnu -lsqlcipher" uv pip install sqlcipher3

# Install other dependencies
RUN uv sync --no-dev
COPY src src
COPY alembic alembic
COPY alembic.ini .

# Stage 2: Runtime Distroless
FROM gcr.io/distroless/python3-debian12@sha256:d8c03cc56c80c2f8f74e643cfc09fcc6c2d1bdf69a4ea5fa3a4dff64cdfa82f4 AS runtime

# Distroless environments lack shell and package managers.
COPY --from=builder /usr/lib/x86_64-linux-gnu/libsqlcipher.so.0 /usr/lib/x86_64-linux-gnu/

COPY --from=builder /app/.venv /app/.venv
COPY --from=builder /app/src /app/src
COPY --from=builder /app/alembic /app/alembic
COPY --from=builder /app/alembic.ini /app/alembic.ini

WORKDIR /app
ENV PYTHONPATH=/app/src
ENV VIRTUAL_ENV=/app/.venv
ENV PATH="/app/.venv/bin:$PATH"

USER 1001

EXPOSE 8000
ENTRYPOINT ["python", "-m", "uvicorn", "acousense.api.main:app", "--host", "0.0.0.0", "--port", "8000"]
