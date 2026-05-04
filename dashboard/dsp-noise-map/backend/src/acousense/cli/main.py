"""
AcouSense — Typer CLI
======================
Production CLI with security-first UX:
  • ``acousense serve``      — Boot all services
  • ``acousense configure``  — Store MQTT credentials in OS keyring
  • ``acousense rotate-key`` — Rotate Fernet encryption key
  • ``acousense prune``      — Delete old readings (natural language dates)
  • ``acousense seed``       — Inject mock data for development
  • ``acousense tui``        — Launch real-time system monitoring dashboard
"""

from __future__ import annotations

import threading
import time
from datetime import datetime, timedelta

import dateparser
import typer
from rich.console import Console

from src.api.server import app as flask_app
from src.core.commands import (
    PruneReadingsCommand,
    SeedMockDataCommand,
    command_history,
)
from src.core.diagnostics import diagnostics
from src.core.mqtt_client import start_mqtt_thread
from src.persistence.database import init_db
from src.security.audit import audit_log

app = typer.Typer(
    help="AcouSense Urban Soundscape Intelligence Platform — CLI",
    add_completion=True,
    rich_markup_mode="rich",
)
console = Console()

# ─────────────────────────────────────────────────────────────────────────────
# Real Background Engine Loop
# ─────────────────────────────────────────────────────────────────────────────


def start_engine_loop() -> None:
    try:
        from src.core.engine import (
            ENGINE_INTERVAL,
            GRID_RESOLUTION,
            engine_cache,
            generate_realtime_heatmap,
        )
        from src.persistence.database import get_db, prune_old_readings

        try:
            import geojsoncontour
        except ImportError:
            geojsoncontour = None

        def generate_contour_geojson(result: dict) -> str | None:
            if geojsoncontour is None or result is None:
                return None
            import numpy as np
            from matplotlib import pyplot as plt

            db_grid = result["grid_db"]
            bbox = result["bbox"]
            min_lat, min_lon, max_lat, max_lon = bbox
            res = result["resolution"]

            lats = np.linspace(min_lat, max_lat, res)
            lons = np.linspace(min_lon, max_lon, res)

            fig, ax = plt.subplots(1, 1, figsize=(1, 1))
            levels = np.arange(30, 95, 5)
            contour = ax.contour(lons, lats, db_grid, levels=levels)
            gj = geojsoncontour.contour_to_geojson(
                contour=contour, min_angle_deg=3.0, ndigits=6, stroke_width=1
            )
            plt.close(fig)
            return gj

        last_prune = time.time()
        print(
            f"[engine] started — grid={GRID_RESOLUTION}x{GRID_RESOLUTION}, "
            f"interval={ENGINE_INTERVAL}s"
        )

        while True:
            try:
                t0 = time.time()

                if t0 - last_prune > 300:
                    prune_old_readings(operator="engine-auto")
                    last_prune = t0

                since_ts = int(
                    (datetime.now() - timedelta(minutes=2)).timestamp() * 1000
                )
                with get_db() as conn:
                    rows = conn.execute(
                        """
                        SELECT sensor_id, AVG(lat) as lat, AVG(lng) as lng,
                               10 * LOG10(AVG(POWER(10, leq_dba / 10.0))) AS leq_dba,
                               MAX(ts) as ts
                        FROM readings WHERE ts >= ? GROUP BY sensor_id
                    """,
                        (since_ts,),
                    ).fetchall()
                readings = [dict(r) for r in rows if r["leq_dba"] is not None]

                if not readings:
                    time.sleep(ENGINE_INTERVAL)
                    continue

                lats = [r["lat"] for r in readings]
                lngs = [r["lng"] for r in readings]
                lat_pad = max((max(lats) - min(lats)) * 0.15, 0.002)
                lng_pad = max((max(lngs) - min(lngs)) * 0.15, 0.002)
                bbox = (
                    min(lats) - lat_pad,
                    min(lngs) - lng_pad,
                    max(lats) + lat_pad,
                    max(lngs) + lng_pad,
                )

                result = generate_realtime_heatmap(readings, bbox)
                contour_gj = generate_contour_geojson(result)
                engine_cache.update(result, contour_gj)

                elapsed = time.time() - t0
                print(
                    f"[engine] grid computed in {elapsed:.2f}s  "
                    f"sensors={len(readings)}"
                )

            except Exception as exc:
                print(f"[engine] error: {exc}")

            time.sleep(ENGINE_INTERVAL)

    except Exception as e:
        print(f"[engine thread crash] {e}")


# ═══════════════════════════════════════════════════════════════════════════════
# Commands
# ═══════════════════════════════════════════════════════════════════════════════


@app.command()
def serve(
    host: str = typer.Option("0.0.0.0", "--host", "-h", help="Bind interface."),
    port: int = typer.Option(5000, "--port", "-p", help="Bind port."),
) -> None:
    """Run all background systems and start the Flask API server."""
    console.print(f"[bold green]Starting AcouSense Server[/] on {host}:{port}")

    init_db()

    # Audit: server start
    audit_log.server_start(host=host, port=port)

    # Start MQTT daemon thread
    start_mqtt_thread()

    # Start background computation loop
    engine_thread = threading.Thread(
        target=start_engine_loop, daemon=True, name="acousense-engine"
    )
    engine_thread.start()

    # Start resource tracking
    diagnostics.start()

    # Block via Flask
    flask_app.run(host=host, port=port)


@app.command()
def api(
    host: str = typer.Option("0.0.0.0", "--host", "-h", help="Bind interface."),
    port: int = typer.Option(8000, "--port", "-p", help="Bind port."),
) -> None:
    """Run the modern FastAPI service layer via Uvicorn."""
    import uvicorn

    console.print(f"[bold green]Starting AcouSense FastAPI Server[/] on {host}:{port}")
    console.print(f"Swagger API Docs available at http://{host}:{port}/docs")
    uvicorn.run("src.api.fastapi_app:app", host=host, port=port, log_level="info")


@app.command()
def configure() -> None:
    """
    Store MQTT broker credentials securely in the OS keyring.

    Passwords are collected with hidden input and confirmation prompts
    to prevent shoulder-surfing and entry errors.
    """
    from src.security.secrets import set_secret

    console.print("[bold cyan]AcouSense — Secure Configuration[/]\n")
    console.print(
        "Credentials will be stored in your OS credential vault "
        "(Windows Credential Manager / macOS Keychain / Linux Secret Service).\n"
    )

    # MQTT Broker
    broker = typer.prompt("MQTT Broker hostname", default="localhost")
    set_secret("MQTT_BROKER", broker)
    audit_log.secret_configured("MQTT_BROKER", via="keyring")

    # MQTT Username (optional)
    has_auth = typer.confirm(
        "Does the MQTT broker require authentication?", default=False
    )
    if has_auth:
        username = typer.prompt("MQTT Username")
        set_secret("MQTT_USERNAME", username)
        audit_log.secret_configured("MQTT_USERNAME", via="keyring")

        # Password — hidden input with confirmation to prevent typos
        password = typer.prompt(
            "MQTT Password",
            hide_input=True,
            confirmation_prompt=True,
        )
        set_secret("MQTT_PASSWORD", password)
        audit_log.secret_configured("MQTT_PASSWORD", via="keyring")
        console.print("[bold green]✓[/] MQTT credentials stored securely.")
    else:
        console.print("[dim]Skipping MQTT authentication setup.[/]")

    console.print("\n[bold green]Configuration complete.[/]")
    console.print("Restart the server with [bold]acousense serve[/] to apply changes.")


@app.command()
def rotate_key() -> None:
    """
    Rotate the AES-256 Fernet encryption key.

    This command will:
      1. Generate a new Fernet key
      2. Re-encrypt all stored sensor_id values in the database
      3. Store the new key in the OS keyring
      4. Log a KEY_ROTATION audit event
    """
    from src.persistence.database import get_db
    from src.security.secrets import (
        _fernet_instance,
        set_secret,
    )

    console.print("[bold yellow]⚠ Key Rotation[/]\n")
    console.print("This will re-encrypt all sensor_id values in the database.")
    typer.confirm("Proceed?", abort=True)

    init_db()

    # Step 1: Read all current sensor_ids and decrypt with old key
    old_fernet = _fernet_instance()
    decrypted_map: dict[int, str] = {}

    with get_db() as conn:
        rows = conn.execute("SELECT id, sensor_id FROM readings").fetchall()
        for row in rows:
            try:
                plaintext = old_fernet.decrypt(row["sensor_id"].encode()).decode()
                decrypted_map[row["id"]] = plaintext
            except Exception:
                # Already plaintext or different key — keep as-is
                decrypted_map[row["id"]] = row["sensor_id"]

    console.print(f"  Decrypted {len(decrypted_map)} sensor_id values.")

    # Step 2: Generate new key and store it
    from cryptography.fernet import Fernet as _Fernet

    new_key = _Fernet.generate_key()
    set_secret("ACOUSENSE_MASTER_KEY", new_key.decode())

    # Force reload of the module-level fernet instance
    import src.security.secrets as _secrets_mod

    _secrets_mod._fernet = _Fernet(new_key)
    new_fernet = _secrets_mod._fernet

    console.print("  New Fernet key generated and stored in keyring.")

    # Step 3: Re-encrypt all sensor_ids with the new key
    with get_db() as conn:
        for row_id, plaintext in decrypted_map.items():
            new_ciphertext = new_fernet.encrypt(plaintext.encode()).decode()
            conn.execute(
                "UPDATE readings SET sensor_id = ? WHERE id = ?",
                (new_ciphertext, row_id),
            )

    audit_log.key_rotation("ACOUSENSE_MASTER_KEY")
    console.print(
        f"[bold green]✓[/] Re-encrypted {len(decrypted_map)} records with new key."
    )


@app.command()
def prune(
    older_than: str = typer.Argument(
        ..., help="Natural language date (e.g. '2 weeks ago', 'yesterday')"
    ),
) -> None:
    """Delete old dataset readings (secure_delete overwrites freed pages)."""
    dt = dateparser.parse(older_than)
    if not dt:
        console.print(f"[bold red]Error:[/] Could not parse date '{older_than}'")
        raise typer.Exit(1)

    delta = datetime.now() - dt
    hours = int(delta.total_seconds() / 3600)

    if hours < 1:
        console.print("[bold yellow]Warning:[/] Will not prune data under 1 hour old.")
        raise typer.Exit(1)

    cmd = PruneReadingsCommand(max_age_hours=hours)
    res = command_history.execute(cmd)

    console.print(f"[bold green]Success:[/] Deleted {res['deleted']} old readings.")
    console.print("[dim]secure_delete=ON — freed pages overwritten with zeros.[/]")


@app.command()
def seed(
    count: int = typer.Option(50, "--count", "-c"),
    sensor: str = typer.Option("ACU-CLI"),
) -> None:
    """Inject mock data sensors for local testing."""
    cmd = SeedMockDataCommand(n=count, sensor_id=sensor)
    res = command_history.execute(cmd)
    console.print(f"[bold blue]Mock Data Seeded:[/] {res['inserted']} items injected.")
    console.print("You can revert this by running undo (via API/TUI directly).")


@app.command()
def tui() -> None:
    """Launch the real-time Textual system monitoring dashboard."""
    from src.cli.tui import AcouSenseTUI

    tui_app = AcouSenseTUI()
    tui_app.run()


if __name__ == "__main__":
    app()
