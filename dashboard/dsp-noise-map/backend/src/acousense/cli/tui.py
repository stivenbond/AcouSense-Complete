"""
AcouSense — Textual TUI
=======================
Real-time dashboard observing the state of the diagnostic buffer, MQTT connections,
and SQLite database footprint.
"""

from __future__ import annotations

import os

from textual.app import App, ComposeResult
from textual.containers import Container, Grid
from textual.widgets import Footer, Header, Log, Static

from src.core.diagnostics import SystemSnapshot, diagnostics
from src.core.mqtt_client import get_mqtt_stats


class StatWidget(Static):
    """Widget to display a key-value metric."""

    def __init__(self, title: str, id: str | None = None) -> None:
        super().__init__(id=id)
        self.title_text = title

    def on_mount(self) -> None:
        self.styles.border = ("round", "gray")

    def update_value(self, value: str) -> None:
        markup = f"[bold]{self.title_text}:[/] {value}"
        self.update(markup)


class AcouSenseTUI(App[None]):
    """The main Textual application dashboard."""

    CSS = """
    Screen {
        background: $surface-darken-1;
    }

    #main-grid {
        grid-size: 3;
        grid-gutter: 1 2;
        padding: 1 2;
    }

    StatWidget {
        height: 5;
        content-align: center middle;
    }

    #log-container {
        column-span: 3;
        height: 1fr;
        border: round $primary;
    }
    
    Log {
        padding: 0 1;
    }
    """

    BINDINGS = [
        ("q", "quit", "Quit"),
        ("c", "clear_log", "Clear Log"),
    ]

    def compose(self) -> ComposeResult:
        yield Header(show_clock=True)
        with Grid(id="main-grid"):
            yield StatWidget("CPU Load", id="cpu-stat")
            yield StatWidget("Memory (RSS)", id="mem-stat")
            yield StatWidget("Engine Age", id="engine-stat")

            yield StatWidget("MQTT Connected", id="mqtt-stat")
            yield StatWidget("MQTT Rx", id="mqtt-rx-stat")
            yield StatWidget("DB Size", id="db-stat")

            with Container(id="log-container"):
                yield Log(id="sys-log", highlight=True)
        yield Footer()

    def on_mount(self) -> None:
        # We start the background diagnostic buffer if not already active
        diagnostics.start()

        self.log_widget = self.query_one("#sys-log", Log)
        self.log_widget.write_line("AcouSense System Monitoring started.")

        # Set up UI update polling (every 1.0s)
        self.set_interval(1.0, self.update_dashboard)

    def action_clear_log(self) -> None:
        self.log_widget.clear()

    def update_dashboard(self) -> None:
        snap: SystemSnapshot | None = diagnostics.latest()
        if snap is not None:
            # CPU
            cpu_w = self.query_one("#cpu-stat", StatWidget)
            cpu_color = (
                "green"
                if snap.cpu_percent < 50
                else ("yellow" if snap.cpu_percent < 85 else "red")
            )
            cpu_w.update_value(f"[{cpu_color}]{snap.cpu_percent:.1f}%[/]")

            # Memory
            mem_w = self.query_one("#mem-stat", StatWidget)
            mem_color = "green" if snap.memory_percent < 70 else "red"
            leak_warning = (
                "[bold red]⚠ LEAK?[/]" if diagnostics.detect_memory_leak(15) else ""
            )
            mem_w.update_value(
                f"[{mem_color}]{snap.memory_rss_mb:.0f} MB[/] {leak_warning}"
            )

            # Engine
            eng_w = self.query_one("#engine-stat", StatWidget)
            age = snap.engine_age_s
            age_color = "green" if age < 10 else ("yellow" if age < 30 else "red")
            eng_w.update_value(f"[{age_color}]{age:.1f} s[/]")

        # MQTT
        mqtt = get_mqtt_stats()
        mqtt_w = self.query_one("#mqtt-stat", StatWidget)
        mqtt_state = "[bold green]YES[/]" if mqtt["connected"] else "[bold red]NO[/]"
        mqtt_w.update_value(mqtt_state)

        rx_w = self.query_one("#mqtt-rx-stat", StatWidget)
        rx_w.update_value(f"{mqtt['messages_received']} msgs")

        # Database File Size
        db_w = self.query_one("#db-stat", StatWidget)
        try:
            from src.persistence.database import DB_PATH

            if os.path.exists(DB_PATH):
                sz_mb = os.path.getsize(DB_PATH) / 1048576
                db_w.update_value(f"{sz_mb:.1f} MB")
            else:
                db_w.update_value("Not found")
        except Exception:
            db_w.update_value("Error")


if __name__ == "__main__":
    AcouSenseTUI().run()
