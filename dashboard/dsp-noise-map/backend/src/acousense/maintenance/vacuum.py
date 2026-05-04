import glob
import os
import shutil
import sqlite3
import time

# Assumed to be running inside the CronJob pod with PVC mounted at /data
DB_PATH = os.getenv("ACOUSENSE_DATA_DIR", "/data/acousense.db")
BACKUP_DIR = "/data/backups"


def run_vacuum():
    print(f"Running maintenance on {DB_PATH}")
    os.makedirs(BACKUP_DIR, exist_ok=True)

    # Pragma wal_checkpoint(TRUNCATE) and VACUUM
    with sqlite3.connect(DB_PATH) as conn:
        conn.execute("PRAGMA wal_checkpoint(TRUNCATE);")
        conn.execute("VACUUM;")

    # Backup
    timestamp = int(time.time())
    backup_path = os.path.join(BACKUP_DIR, f"acousense_{timestamp}.db")
    shutil.copy2(DB_PATH, backup_path)
    print(f"Backed up to {backup_path}")

    # Prune older than 30 days
    limit = time.time() - (30 * 24 * 60 * 60)
    for f in glob.glob(os.path.join(BACKUP_DIR, "*.db")):
        if os.path.getmtime(f) < limit:
            os.remove(f)
            print(f"Pruned old backup {f}")


if __name__ == "__main__":
    run_vacuum()
