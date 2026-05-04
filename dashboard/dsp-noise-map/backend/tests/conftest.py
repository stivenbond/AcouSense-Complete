from collections.abc import AsyncGenerator

import aiosqlite
import pytest
import pytest_asyncio
from acousense.api.main import app
from acousense.db.connection import get_db
from httpx import ASGITransport, AsyncClient


@pytest.fixture(autouse=True)
def mock_env(monkeypatch):
    monkeypatch.setenv("SQLCIPHER_DISABLED", "1")
    monkeypatch.setenv("ACOUSENSE_API_KEY", "ci-test-secret")
    monkeypatch.setenv("ACOUSENSE_SQLCIPHER_PASSPHRASE", "ci-test-test")


@pytest_asyncio.fixture(scope="session")
async def db() -> AsyncGenerator[aiosqlite.Connection, None]:
    async with aiosqlite.connect(":memory:") as conn:
        await conn.execute("PRAGMA journal_mode=WAL;")
        await conn.execute("PRAGMA synchronous=NORMAL;")

        # Manually run the initial schema since Alembic doesn't run easily against :memory: without special env
        await conn.execute("""
            CREATE TABLE items (
                id INTEGER PRIMARY KEY,
                name VARCHAR(255) NOT NULL,
                description TEXT,
                timestamp VARCHAR(50) NOT NULL,
                deleted_at VARCHAR(50)
            )
        """)
        await conn.execute("""
            CREATE TABLE commands (
                id INTEGER PRIMARY KEY,
                operation_type VARCHAR(50) NOT NULL,
                payload_json TEXT NOT NULL,
                executed_at VARCHAR(50) NOT NULL,
                undone_at VARCHAR(50),
                session_id VARCHAR(100)
            )
        """)

        async def override_get_db():
            yield conn

        app.dependency_overrides[get_db] = override_get_db
        yield conn
        app.dependency_overrides.clear()


@pytest_asyncio.fixture
async def async_client(db) -> AsyncGenerator[AsyncClient, None]:
    async with AsyncClient(
        transport=ASGITransport(app=app),
        base_url="http://test",
        headers={"X-API-Key": "ci-test-secret"},
    ) as client:
        yield client
