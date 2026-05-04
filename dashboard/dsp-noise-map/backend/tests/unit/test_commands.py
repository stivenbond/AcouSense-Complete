import pytest
from acousense.core.commands import CommandHistory
from acousense.services.items import CreateItemCommand


@pytest.mark.asyncio
async def test_command_execute_and_undo(db):
    history = CommandHistory(session_id="test_session")
    item_data = {
        "name": "test1",
        "description": "desc1",
        "timestamp": "2025-01-01T00:00:00+00:00",
        "deleted_at": None,
    }

    cmd = CreateItemCommand(1, item_data)
    await history.push(db, cmd)

    # Assert executed
    async with db.execute("SELECT name FROM items WHERE id = 1") as cur:
        row = await cur.fetchone()
        assert row is not None
        assert row[0] == "test1"

    # Assert history recorded
    async with db.execute(
        "SELECT operation_type FROM commands WHERE session_id = 'test_session'"
    ) as cur:
        row = await cur.fetchone()
        assert row[0] == "CREATE_ITEM"

    # Undo
    await history.undo_last(db)
    async with db.execute("SELECT name FROM items WHERE id = 1") as cur:
        row = await cur.fetchone()
        assert row is None
