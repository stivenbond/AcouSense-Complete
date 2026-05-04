from datetime import UTC, datetime
from typing import Any

import aiosqlite
import structlog
from acousense.core.commands import Command, CommandHistory, register_command

logger = structlog.get_logger(__name__)


@register_command("CREATE_ITEM")
class CreateItemCommand(Command):
    def __init__(self, item_id: int, item_data: dict[str, Any]):
        self.item_id = item_id
        self.item_data = item_data

    async def execute(self, db: aiosqlite.Connection) -> None:
        await db.execute(
            """INSERT INTO items (id, name, description, timestamp, deleted_at)
               VALUES (?, ?, ?, ?, ?)""",
            (
                self.item_id,
                self.item_data.get("name"),
                self.item_data.get("description"),
                self.item_data.get("timestamp"),
                self.item_data.get("deleted_at"),
            ),
        )
        logger.info("Item created", item_id=self.item_id)

    async def undo(self, db: aiosqlite.Connection) -> None:
        await db.execute("DELETE FROM items WHERE id = ?", (self.item_id,))
        logger.info("Item creation undone", item_id=self.item_id)

    @property
    def operation_type(self) -> str:
        return "CREATE_ITEM"

    @property
    def payload(self) -> dict[str, Any]:
        return {"item_id": self.item_id, "item_data": self.item_data}


@register_command("UPDATE_ITEM")
class UpdateItemCommand(Command):
    def __init__(
        self, item_id: int, old_data: dict[str, Any], new_data: dict[str, Any]
    ):
        self.item_id = item_id
        self.old_data = old_data
        self.new_data = new_data

    async def execute(self, db: aiosqlite.Connection) -> None:
        updates = []
        params = []
        for k, v in self.new_data.items():
            updates.append(f"{k} = ?")
            params.append(v)

        if not updates:
            return

        params.append(self.item_id)
        query = f"UPDATE items SET {', '.join(updates)} WHERE id = ?"
        await db.execute(query, tuple(params))

    async def undo(self, db: aiosqlite.Connection) -> None:
        updates = []
        params = []
        for k, v in self.old_data.items():
            updates.append(f"{k} = ?")
            params.append(v)

        if not updates:
            return

        params.append(self.item_id)
        query = f"UPDATE items SET {', '.join(updates)} WHERE id = ?"
        await db.execute(query, tuple(params))

    @property
    def operation_type(self) -> str:
        return "UPDATE_ITEM"

    @property
    def payload(self) -> dict[str, Any]:
        return {
            "item_id": self.item_id,
            "old_data": self.old_data,
            "new_data": self.new_data,
        }


@register_command("DELETE_ITEM")
class DeleteItemCommand(Command):
    def __init__(self, item_id: int, deleted_at: str):
        self.item_id = item_id
        self.deleted_at = deleted_at

    async def execute(self, db: aiosqlite.Connection) -> None:
        await db.execute(
            "UPDATE items SET deleted_at = ? WHERE id = ?",
            (self.deleted_at, self.item_id),
        )

    async def undo(self, db: aiosqlite.Connection) -> None:
        await db.execute(
            "UPDATE items SET deleted_at = NULL WHERE id = ?", (self.item_id,)
        )

    @property
    def operation_type(self) -> str:
        return "DELETE_ITEM"

    @property
    def payload(self) -> dict[str, Any]:
        return {"item_id": self.item_id, "deleted_at": self.deleted_at}


class ItemService:
    def __init__(self, db: aiosqlite.Connection, command_history: CommandHistory):
        self.db = db
        self.history = command_history

    async def get_items(
        self, limit: int = 10, offset: int = 0, sort: str = "id", db_filter: str = None
    ) -> list[dict[str, Any]]:
        # In a real scenario, use query builder. For now, simplistic parameterization.
        query = f"SELECT id, name, description, timestamp, deleted_at FROM items WHERE deleted_at IS NULL ORDER BY {sort} LIMIT ? OFFSET ?"
        async with self.db.execute(query, (limit, offset)) as cur:
            rows = await cur.fetchall()
            return [
                {"id": r[0], "name": r[1], "description": r[2], "timestamp": r[3]}
                for r in rows
            ]

    async def get_item(self, item_id: int) -> dict[str, Any] | None:
        async with self.db.execute(
            "SELECT id, name, description, timestamp, deleted_at FROM items WHERE id = ? AND deleted_at IS NULL",
            (item_id,),
        ) as cur:
            row = await cur.fetchone()
            if row:
                return {
                    "id": row[0],
                    "name": row[1],
                    "description": row[2],
                    "timestamp": row[3],
                }
            return None

    async def create_item(self, name: str, description: str, timestamp: str) -> int:
        async with self.db.execute("SELECT COALESCE(MAX(id), 0) + 1 FROM items") as cur:
            row = await cur.fetchone()
            new_id = row[0]

        item_data = {
            "name": name,
            "description": description,
            "timestamp": timestamp,
            "deleted_at": None,
        }
        cmd = CreateItemCommand(new_id, item_data)
        await self.history.push(self.db, cmd)
        return new_id

    async def update_item(self, item_id: int, new_data: dict[str, Any]) -> bool:
        item = await self.get_item(item_id)
        if not item:
            return False

        old_data = {k: item[k] for k in new_data.keys() if k in item}
        cmd = UpdateItemCommand(item_id, old_data, new_data)
        await self.history.push(self.db, cmd)
        return True

    async def delete_item(self, item_id: int) -> bool:
        item = await self.get_item(item_id)
        if not item:
            return False

        now = datetime.now(UTC).isoformat()
        cmd = DeleteItemCommand(item_id, now)
        await self.history.push(self.db, cmd)
        return True
