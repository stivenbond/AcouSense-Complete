import os
from logging.config import fileConfig

from acousense.db.connection import DB_PATH
from acousense.utils.secrets import get_secret
from alembic import context
from sqlalchemy import create_engine, event

config = context.config

if config.config_file_name is not None:
    fileConfig(config.config_file_name, disable_existing_loggers=False)

target_metadata = None


def get_engine():
    engine = create_engine(f"sqlite:///{DB_PATH}")

    @event.listens_for(engine, "connect")
    def set_sqlite_pragma(dbapi_connection, connection_record):
        encryption_disabled = os.getenv("SQLCIPHER_DISABLED", "0") == "1"
        if not encryption_disabled:
            try:
                passphrase = get_secret("acousense", "sqlcipher-passphrase")
                cursor = dbapi_connection.cursor()
                cursor.execute(f"PRAGMA key='{passphrase}';")
                cursor.close()
            except Exception:
                pass

        cursor = dbapi_connection.cursor()
        cursor.execute("PRAGMA journal_mode=WAL;")
        cursor.execute("PRAGMA synchronous=NORMAL;")
        cursor.execute("PRAGMA foreign_keys=ON;")
        cursor.execute("PRAGMA busy_timeout=5000;")
        cursor.close()

    return engine


def run_migrations_offline() -> None:
    context.configure(
        url=f"sqlite:///{DB_PATH}",
        target_metadata=target_metadata,
        literal_binds=True,
        dialect_opts={"paramstyle": "named"},
    )
    with context.begin_transaction():
        context.run_migrations()


def run_migrations_online() -> None:
    engine = get_engine()
    with engine.connect() as connection:
        context.configure(connection=connection, target_metadata=target_metadata)
        with context.begin_transaction():
            context.run_migrations()


if context.is_offline_mode():
    run_migrations_offline()
else:
    run_migrations_online()
