from pathlib import Path
import shutil
from typing import Tuple

Import("env")


def resolve_repo_and_firmware_roots(project_dir: Path) -> Tuple[Path, Path]:
    """
    PLATFORMIO PROJECT_DIR may be repo root …/AcouSense (nested layout) or
    …/AcouSense/firmware (standalone firmware project).

    Repo root contains shared/, firmware/arduino/, firmware/esp32/.
    """
    repo = Path(project_dir)
    if (
        repo.is_dir()
        and (repo.parent / "shared").is_dir()
        and repo.name == "firmware"
    ):
        return repo.parent, repo
    if (repo / "shared").is_dir() and (repo / "firmware" / "arduino").is_dir():
        return repo, repo / "firmware"
    raise RuntimeError(
        "sync_shared_headers: could not resolve repo/firmware layout from "
        f"PROJECT_DIR={project_dir}"
    )


REPO_ROOT, FIRMWARE_ROOT = resolve_repo_and_firmware_roots(
    Path(env["PROJECT_DIR"])
)
SHARED = REPO_ROOT / "shared"
ARDUINO_DIR = FIRMWARE_ROOT / "arduino"
ESP32_DIR = FIRMWARE_ROOT / "esp32"

SYNC_MAP = {
    SHARED / "packet_protocol.h": [
        ARDUINO_DIR / "packet_protocol.h",
        ESP32_DIR / "packet_protocol.h",
    ],
    SHARED / "ble_constants.h": [
        ESP32_DIR / "ble_constants.h",
    ],
    SHARED / "exposure_classes.h": [
        ESP32_DIR / "exposure_classes.h",
    ],
}


def sync_file(src: Path, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    if dest.exists() and dest.read_bytes() == src.read_bytes():
        return
    shutil.copy2(src, dest)
    print(f"[sync_shared_headers] {src.relative_to(REPO_ROOT)} -> {dest.relative_to(REPO_ROOT)}")


for source, destinations in SYNC_MAP.items():
    for destination in destinations:
        sync_file(source, destination)
