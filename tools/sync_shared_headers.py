from pathlib import Path
import shutil

Import("env")


ROOT = Path(env["PROJECT_DIR"])
SHARED = ROOT / "shared"
ARDUINO_DIR = ROOT / "firmware" / "arduino"
ESP32_DIR = ROOT / "firmware" / "esp32"


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
    print(f"[sync_shared_headers] {src.relative_to(ROOT)} -> {dest.relative_to(ROOT)}")


for source, destinations in SYNC_MAP.items():
    for destination in destinations:
        sync_file(source, destination)
