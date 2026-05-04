import asyncio
import json
import os
from datetime import datetime

import aiosqlite
import dateparser
import typer
from rich import box
from rich.console import Console
from rich.progress import Progress
from rich.table import Table

from acousense.core.commands import CommandHistory
from acousense.db.connection import DB_PATH
from acousense.services.items import ItemService

app = typer.Typer(
    help="AcouSense Urban Soundscape Intelligence Platform CLI", add_completion=True
)
err_console = Console(stderr=True)
out_console = Console()


class GlobalContext:
    def __init__(self):
        self.verbose = False
        self.output_format = "table"


global_ctx = GlobalContext()


def parse_flexible_date(raw: str) -> datetime:
    parsed = dateparser.parse(raw, settings={"TIMEZONE": "UTC"})
    if not parsed:
        raise typer.BadParameter(
            f"Could not parse '{raw}'. Try '3 days ago', 'next Friday 9am', or ISO 8601."
        )
    return parsed


@app.callback()
def main_callback(
    verbose: bool = typer.Option(
        False, "--verbose", "-v", help="Enable verbose output"
    ),
    quiet: bool = typer.Option(
        False, "--quiet", "-q", help="Suppress all non-error output"
    ),
    output: str = typer.Option(
        "table", "--output", "-o", help="Output format: json, table, csv"
    ),
):
    global_ctx.verbose = verbose
    global_ctx.output_format = output if not quiet else "none"

    # Alembic auto-migration check
    from alembic import command
    from alembic.config import Config

    try:
        if verbose:
            err_console.print("[dim]Checking database schema...[/dim]")

        alembic_ini_path = os.path.join(os.getcwd(), "alembic.ini")
        if os.path.exists(alembic_ini_path):
            alembic_cfg = Config(alembic_ini_path)
            alembic_cfg.set_main_option(
                "script_location", "src/acousense/db/migrations"
            )
            command.upgrade(alembic_cfg, "head")
    except Exception as e:
        err_console.print(f"[bold red]Migration Error:[/] {e}")
        raise typer.Exit(1)


def print_items(items):
    if global_ctx.output_format == "none":
        return

    if global_ctx.output_format == "json":
        json_output = json.dumps(items, indent=2)
        out_console.print(json_output, markup=False)
        return

    if global_ctx.output_format == "csv":
        import csv
        import sys

        writer = csv.DictWriter(
            sys.stdout, fieldnames=["id", "name", "description", "timestamp"]
        )
        writer.writeheader()
        for item in items:
            writer.writerow(
                {k: item.get(k) for k in ["id", "name", "description", "timestamp"]}
            )
        return

    if global_ctx.output_format == "table":
        table = Table(box=box.ROUNDED, show_lines=True)
        table.add_column("ID", justify="right", style="cyan", no_wrap=True)
        table.add_column("Name", style="magenta")
        table.add_column("Timestamp", style="green")
        table.add_column("Description")

        for i, item in enumerate(items):
            style = "on #1e1e1e" if i % 2 == 0 else ""
            table.add_row(
                str(item["id"]),
                item["name"],
                item["timestamp"],
                item["description"] or "",
                style=style,
            )

        out_console.print(table)


@app.command()
def list_items(limit: int = 10):
    """List noise measurement items."""

    async def _list():
        async with aiosqlite.connect(DB_PATH) as db:
            service = ItemService(db, CommandHistory())
            return await service.get_items(limit=limit)

    items = asyncio.run(_list())
    print_items(items)


@app.command()
def create(name: str, description: str, timestamp: str):
    """Create a new noise measurement entry."""
    dt = parse_flexible_date(timestamp)

    async def _create():
        async with aiosqlite.connect(DB_PATH) as db:
            service = ItemService(db, CommandHistory())
            new_id = await service.create_item(name, description, dt.isoformat())
            return new_id

    with Progress() as progress:
        task = progress.add_task("[green]Creating...", total=100)
        new_id = asyncio.run(_create())
        progress.update(task, completed=100)

    out_console.print(f"Created Item [bold cyan]#{new_id}[/]")


@app.command()
def undo():
    """Undo the last operation."""

    async def _undo():
        async with aiosqlite.connect(DB_PATH) as db:
            service = ItemService(db, CommandHistory())
            return await service.history.undo_last(db)

    success = asyncio.run(_undo())
    if success:
        out_console.print("[green]Last operation undone successfully.[/]")
    else:
        err_console.print("[bold red]Error:[/] No operations to undo.")


@app.command()
def tui():
    """Launch the Textual Dashboard."""
    from acousense.tui.app import AcouSenseApp

    app = AcouSenseApp()
    app.run()


if __name__ == "__main__":
    app()
