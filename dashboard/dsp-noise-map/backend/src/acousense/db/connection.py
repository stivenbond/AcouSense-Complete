import os
import sqlite3
from collections.abc import AsyncGenerator

import aiosqlite
import structlog
from acousense.utils.secrets import get_secret

logger = structlog.get_logger(__name__)

DB_PATH = os.path.join(os.getenv("ACOUSENSE_DATA_DIR", "."), "acousense.db")


async def get_db() -> AsyncGenerator[aiosqlite.Connection, None]:
    """Dependency injection for FastAPI and other async flows."""
    async with aiosqlite.connect(DB_PATH, isolation_level=None) as db:
        await configure_connection(db)
        yield db


async def configure_connection(db: aiosqlite.Connection) -> None:
    # SQLCipher Encryption Setup
    encryption_disabled = os.getenv("SQLCIPHER_DISABLED", "0") == "1"
    if not encryption_disabled:
        try:
            passphrase = get_secret("acousense", "sqlcipher-passphrase")
            await db.execute(f"PRAGMA key='{passphrase}';")
        except Exception as e:
            logger.warning(
                "Could not set SQLCipher key. Proceeding in plaintext.", error=str(e)
            )

    # 2025-standard SQLite production tweaks
    await db.execute("PRAGMA journal_mode=WAL;")
    await db.execute("PRAGMA synchronous=NORMAL;")
    await db.execute("PRAGMA foreign_keys=ON;")
    await db.execute("PRAGMA busy_timeout=5000;")


def get_sync_db() -> sqlite3.Connection:
    """Synchronous connection for Typer app and plain dbapi requirements."""
    conn = sqlite3.connect(DB_PATH, isolation_level=None)
    encryption_disabled = os.getenv("SQLCIPHER_DISABLED", "0") == "1"
    if not encryption_disabled:
        try:
            passphrase = get_secret("acousense", "sqlcipher-passphrase")
            conn.execute(f"PRAGMA key='{passphrase}';")
        except Exception as e:
            logger.warning("Could not set SQLCipher key synchronously.", error=str(e))

    conn.execute("PRAGMA journal_mode=WAL;")
    conn.execute("PRAGMA synchronous=NORMAL;")
    conn.execute("PRAGMA foreign_keys=ON;")
    conn.execute("PRAGMA busy_timeout=5000;")
    return conn
