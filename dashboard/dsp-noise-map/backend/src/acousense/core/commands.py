import abc
import json
import os
from datetime import UTC, datetime
from typing import Any

import aiosqlite
import structlog

logger = structlog.get_logger(__name__)


class Command(abc.ABC):
    @abc.abstractmethod
    async def execute(self, db: aiosqlite.Connection) -> None:
        pass

    @abc.abstractmethod
    async def undo(self, db: aiosqlite.Connection) -> None:
        pass

    @property
    @abc.abstractmethod
    def operation_type(self) -> str:
        """Returns string identifier of the command type."""
        pass

    @property
    @abc.abstractmethod
    def payload(self) -> dict[str, Any]:
        """Returns JSON-serializable state necessary for undo/redo."""
        pass


_COMMAND_REGISTRY: dict[str, type[Command]] = {}


def register_command(op_type: str):
    def wrapper(cls: type[Command]):
        _COMMAND_REGISTRY[op_type] = cls
        return cls

    return wrapper


class CommandHistory:
    def __init__(self, session_id: str = "default_cli", max_depth: int = 50):
        self.session_id = session_id
        self.max_depth = int(os.getenv("UNDO_STACK_MAX", str(max_depth)))

    async def push(self, db: aiosqlite.Connection, cmd: Command) -> None:
        await cmd.execute(db)

        now = datetime.now(UTC).isoformat()
        payload_data = json.dumps(cmd.payload)

        await db.execute(
            """DELETE FROM commands 
               WHERE session_id = ? AND undone_at IS NOT NULL""",
            (self.session_id,),
        )

        await db.execute(
            """INSERT INTO commands (operation_type, payload_json, executed_at, session_id)
               VALUES (?, ?, ?, ?)""",
            (cmd.operation_type, payload_data, now, self.session_id),
        )

        await db.execute(
            """DELETE FROM commands 
               WHERE session_id = ? AND id NOT IN (
                   SELECT id FROM commands 
                   WHERE session_id = ? 
                   ORDER BY id DESC LIMIT ?
               )""",
            (self.session_id, self.session_id, self.max_depth),
        )

    async def undo_last(self, db: aiosqlite.Connection) -> bool:
        async with db.execute(
            """SELECT id, operation_type, payload_json 
               FROM commands 
               WHERE session_id = ? AND undone_at IS NULL
               ORDER BY id DESC LIMIT 1""",
            (self.session_id,),
        ) as cur:
            row = await cur.fetchone()

        if not row:
            return False

        cmd_id, op_type, payload_json = row
        payload = json.loads(payload_json)

        cls = _COMMAND_REGISTRY.get(op_type)
        if not cls:
            logger.error("Command not in registry", op=op_type)
            return False

        cmd = cls(**payload)  # type: ignore
        await cmd.undo(db)

        now = datetime.now(UTC).isoformat()
        await db.execute(
            "UPDATE commands SET undone_at = ? WHERE id = ?", (now, cmd_id)
        )
        return True

    async def redo_last(self, db: aiosqlite.Connection) -> bool:
        async with db.execute(
            """SELECT id, operation_type, payload_json 
               FROM commands 
               WHERE session_id = ? AND undone_at IS NOT NULL
               ORDER BY id ASC LIMIT 1""",
            (self.session_id,),
        ) as cur:
            row = await cur.fetchone()

        if not row:
            return False

        cmd_id, op_type, payload_json = row
        payload = json.loads(payload_json)

        cls = _COMMAND_REGISTRY.get(op_type)
        if not cls:
            return False

        cmd = cls(**payload)  # type: ignore
        await cmd.execute(db)

        await db.execute("UPDATE commands SET undone_at = NULL WHERE id = ?", (cmd_id,))
        return True

    async def get_history(
        self, db: aiosqlite.Connection, limit: int = 50, offset: int = 0
    ):
        async with db.execute(
            """SELECT id, operation_type, executed_at, undone_at 
               FROM commands 
               WHERE session_id = ?
               ORDER BY id DESC
               LIMIT ? OFFSET ?""",
            (self.session_id, limit, offset),
        ) as cur:
            rows = await cur.fetchall()

        return [
            {"id": r[0], "operation_type": r[1], "executed_at": r[2], "undone_at": r[3]}
            for r in rows
        ]
