"""
AcouSense — Diagnostics
========================
Real-time resource monitoring tooling surfaced by the Textual TUI.

Monitors:
  • CPU usage  (per-core + aggregate, psutil)
  • Memory     (RSS / VMS with leak trend detection)
  • Disk I/O   (reads/writes/bytes deltas from psutil)
  • Engine lag (how old the last heatmap computation is)
"""

from __future__ import annotations

import os
import threading
import time
from dataclasses import dataclass, field
from typing import Any

try:
    import psutil  # type: ignore[import]

    HAS_PSUTIL = True
    _PROC = psutil.Process(os.getpid())
except ImportError:
    HAS_PSUTIL = False
    _PROC = None  # type: ignore[assignment]


# ─────────────────────────────────────────────────────────────────────────────
# Data models
# ─────────────────────────────────────────────────────────────────────────────


@dataclass
class SystemSnapshot:
    """Immutable point-in-time system resource snapshot."""

    timestamp: float = field(default_factory=time.time)
    cpu_percent: float = 0.0
    cpu_per_core: list[float] = field(default_factory=list)
    memory_rss_mb: float = 0.0
    memory_vms_mb: float = 0.0
    memory_percent: float = 0.0
    disk_read_mb_s: float = 0.0
    disk_write_mb_s: float = 0.0
    engine_age_s: float = 0.0  # seconds since last heatmap
    engine_sensor_count: int = 0


# ─────────────────────────────────────────────────────────────────────────────
# Collector
# ─────────────────────────────────────────────────────────────────────────────


class DiagnosticsCollector:
    """
    Background collector that polls psutil every *interval* seconds
    and maintains a ring buffer of the last *history_len* snapshots.

    Thread-safe: all public methods may be called from any thread.
    """

    def __init__(self, interval: float = 2.0, history_len: int = 60) -> None:
        self._interval = interval
        self._history_len = history_len
        self._lock = threading.Lock()
        self._history: list[SystemSnapshot] = []
        self._prev_disk_counters: Any = None
        self._prev_disk_time: float = time.time()
        self._running = False
        self._thread: threading.Thread | None = None

    # ── Public API ────────────────────────────────────────────────────────────

    def start(self) -> None:
        """Start the background collection thread."""
        if self._running:
            return
        self._running = True
        self._thread = threading.Thread(
            target=self._loop, name="acousense-diagnostics", daemon=True
        )
        self._thread.start()

    def stop(self) -> None:
        """Signal the collection thread to stop."""
        self._running = False

    def latest(self) -> SystemSnapshot | None:
        """Return the most recent snapshot, or None if no data yet."""
        with self._lock:
            return self._history[-1] if self._history else None

    def history(self, n: int = 30) -> list[SystemSnapshot]:
        """Return the last *n* snapshots, oldest first."""
        with self._lock:
            return list(self._history[-n:])

    def detect_memory_leak(self, window: int = 20) -> bool:
        """
        Heuristic: return True if RSS memory has risen monotonically
        over the last *window* snapshots (potential leak signal).
        """
        snaps = self.history(window)
        if len(snaps) < window:
            return False
        rss_values = [s.memory_rss_mb for s in snaps]
        return all(
            rss_values[i] < rss_values[i + 1] for i in range(len(rss_values) - 1)
        )

    # ── Collection loop ───────────────────────────────────────────────────────

    def _loop(self) -> None:
        while self._running:
            snap = self._collect()
            with self._lock:
                self._history.append(snap)
                if len(self._history) > self._history_len:
                    self._history.pop(0)
            time.sleep(self._interval)

    def _collect(self) -> SystemSnapshot:
        if not HAS_PSUTIL or _PROC is None:
            return SystemSnapshot()

        # CPU
        cpu_total = psutil.cpu_percent(interval=None)
        cpu_cores = psutil.cpu_percent(interval=None, percpu=True)

        # Memory
        mem = _PROC.memory_info()
        mem_total = psutil.virtual_memory()

        # Disk I/O (delta)
        disk_read_mb_s = 0.0
        disk_write_mb_s = 0.0
        try:
            current_disk = psutil.disk_io_counters()
            now = time.time()
            if self._prev_disk_counters is not None and current_disk is not None:
                dt = max(now - self._prev_disk_time, 0.001)
                disk_read_mb_s = (
                    (current_disk.read_bytes - self._prev_disk_counters.read_bytes)
                    / dt
                    / 1_048_576
                )
                disk_write_mb_s = (
                    (current_disk.write_bytes - self._prev_disk_counters.write_bytes)
                    / dt
                    / 1_048_576
                )
            self._prev_disk_counters = current_disk
            self._prev_disk_time = now
        except Exception:
            pass

        # Engine age — imported lazily to avoid circular imports at module level
        engine_age_s = 0.0
        engine_sensor_count = 0
        try:
            from src.core.engine import engine_cache  # noqa: PLC0415

            result, _, _ = engine_cache.snapshot()
            if result is not None:
                engine_age_s = time.time() - result["timestamp"]
        except Exception:
            pass

        return SystemSnapshot(
            cpu_percent=cpu_total,
            cpu_per_core=list(cpu_cores)
            if isinstance(cpu_cores, list)
            else [cpu_total],
            memory_rss_mb=mem.rss / 1_048_576,
            memory_vms_mb=mem.vms / 1_048_576,
            memory_percent=mem_total.percent,
            disk_read_mb_s=disk_read_mb_s,
            disk_write_mb_s=disk_write_mb_s,
            engine_age_s=engine_age_s,
            engine_sensor_count=engine_sensor_count,
        )


# ── Module-level singleton for use by CLI/TUI ─────────────────────────────
diagnostics = DiagnosticsCollector(interval=2.0, history_len=60)
