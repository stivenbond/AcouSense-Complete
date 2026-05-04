import aiosqlite
from acousense.core.commands import CommandHistory
from acousense.db.connection import DB_PATH
from acousense.services.items import ItemService
from textual.app import App, ComposeResult
from textual.binding import Binding
from textual.containers import Horizontal
from textual.widgets import DataTable, Footer, Header, Input, Static


class Sidebar(Static):
    def compose(self) -> ComposeResult:
        yield Input(placeholder="Search via '/'...", id="search_input")
        yield Static("Filters & Search Placeholder", id="sidebar_content")


class AcouSenseApp(App):
    CSS_PATH = "app.tcss"
    TITLE = "AcouSense Dashboard"
    BINDINGS = [
        Binding("q", "quit", "Quit"),
        Binding("r", "refresh", "Refresh"),
        Binding("u", "undo", "Undo"),
        Binding("ctrl+z", "undo", "Undo"),
        Binding("slash", "focus_search", "Search"),
    ]

    def compose(self) -> ComposeResult:
        yield Header(show_clock=True)
        with Horizontal():
            yield Sidebar(id="sidebar")
            yield DataTable(id="items_table")
        yield Footer()

    async def on_mount(self) -> None:
        table = self.query_one(DataTable)
        table.add_columns("ID", "Name", "Description", "Timestamp")
        table.cursor_type = "row"
        self.call_after_refresh(self.load_data)

    async def load_data(self) -> None:
        table = self.query_one(DataTable)
        table.clear(columns=False)

        async with aiosqlite.connect(DB_PATH) as db:
            service = ItemService(db, CommandHistory())
            items = await service.get_items(limit=50)

        for item in items:
            table.add_row(
                str(item["id"]),
                item["name"],
                item["description"] or "",
                item["timestamp"],
                key=str(item["id"]),
            )

    async def action_refresh(self) -> None:
        self.notify("Refreshing data...")
        await self.load_data()

    async def action_undo(self) -> None:
        self.notify("Attempting undo...")
        async with aiosqlite.connect(DB_PATH) as db:
            service = ItemService(db, CommandHistory())
            success = await service.history.undo_last(db)
        if success:
            self.notify("Undo successful.")
            await self.load_data()
        else:
            self.notify("Nothing to undo.", severity="error")

    def action_focus_search(self) -> None:
        input_widget = self.query_one("#search_input", Input)
        input_widget.focus()


if __name__ == "__main__":
    app = AcouSenseApp()
    app.run()
