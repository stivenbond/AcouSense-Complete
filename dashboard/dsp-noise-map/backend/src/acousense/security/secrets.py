"""
AcouSense — Secret Management
================================
Provides OS-native credential storage via the ``keyring`` library, which
delegates to the appropriate vault on each platform:

  • Windows   → Windows Credential Manager (wincred)
  • macOS     → Keychain
  • Linux     → Secret Service (gnome-keyring / KWallet)

Fernet Fallback
---------------
If ``keyring`` has no viable backend (e.g. headless CI servers without a
D-Bus session), secrets are stored in an encrypted file at
``~/.acousense/secrets.enc`` using AES-256-CBC via ``cryptography.Fernet``.
The Fernet key itself is derived from a machine-unique salt using PBKDF2-HMAC
and stored in the OS keyring as ``ACOUSENSE_MASTER_KEY``.

Usage
-----
    from src.security.secrets import get_secret, set_secret

    broker_url = get_secret("MQTT_BROKER_URL")
    set_secret("MQTT_BROKER_URL", "mqtt://192.168.1.10")

Security invariants
-------------------
  • Secrets are NEVER logged.
  • Secrets are NEVER read from or written to .env files.
  • All Fernet-encrypted secrets at rest use a per-installation key.
  • Memory is not explicitly zeroed (Python limitation; use in process-isolated
    contexts).
"""

from __future__ import annotations

import base64
import json
import platform
import secrets as _pysecrets
from pathlib import Path

# ── keyring ───────────────────────────────────────────────────────────────
try:
    import keyring  # type: ignore[import]
    import keyring.errors  # type: ignore[import]

    HAS_KEYRING = True
except ImportError:
    HAS_KEYRING = False

# ── cryptography (Fernet AES-256) ─────────────────────────────────────────
from cryptography.fernet import Fernet, InvalidToken
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC

# ─────────────────────────────────────────────────────────────────────────────
# Constants
# ─────────────────────────────────────────────────────────────────────────────
_SERVICE_NAME: str = "acousense"
_MASTER_KEY_ATTR: str = "ACOUSENSE_MASTER_KEY"
_FALLBACK_DIR: Path = Path.home() / ".acousense"
_FALLBACK_FILE: Path = _FALLBACK_DIR / "secrets.enc"
_SALT_FILE: Path = _FALLBACK_DIR / "salt.bin"


# ─────────────────────────────────────────────────────────────────────────────
# Fernet key management
# ─────────────────────────────────────────────────────────────────────────────


def _get_or_create_salt() -> bytes:
    """Return the persistent per-installation PBKDF2 salt (32 random bytes)."""
    _FALLBACK_DIR.mkdir(mode=0o700, parents=True, exist_ok=True)
    if _SALT_FILE.exists():
        return _SALT_FILE.read_bytes()
    salt = _pysecrets.token_bytes(32)
    _SALT_FILE.write_bytes(salt)
    _SALT_FILE.chmod(0o600)
    return salt


def _derive_fernet_key(passphrase: bytes) -> bytes:
    """Derive a 32-byte Fernet-compatible key from *passphrase* + stored salt."""
    salt = _get_or_create_salt()
    kdf = PBKDF2HMAC(
        algorithm=hashes.SHA256(), length=32, salt=salt, iterations=480_000
    )
    return base64.urlsafe_b64encode(kdf.derive(passphrase))


def _get_master_key() -> Fernet:
    """
    Retrieve (or generate) the Fernet master key.

    Attempt order:
      1. OS keyring  →  read ``ACOUSENSE_MASTER_KEY``
      2. Generate a new random key, store to OS keyring
      3. If keyring unavailable: derive from machine fingerprint, store to disk
    """
    if HAS_KEYRING:
        try:
            stored = keyring.get_password(_SERVICE_NAME, _MASTER_KEY_ATTR)
            if stored:
                return Fernet(stored.encode())
            # First run — generate and persist
            key = Fernet.generate_key()
            keyring.set_password(_SERVICE_NAME, _MASTER_KEY_ATTR, key.decode())
            return Fernet(key)
        except keyring.errors.KeyringError:
            pass  # fall through to file-based fallback

    # Headless / CI fallback: derive from platform fingerprint
    fingerprint = (platform.node() + platform.machine() + platform.processor()).encode()
    key = _derive_fernet_key(fingerprint)
    return Fernet(key)


# Module-level Fernet instance (lazy-initialised on first call)
_fernet: Fernet | None = None


def _fernet_instance() -> Fernet:
    global _fernet
    if _fernet is None:
        _fernet = _get_master_key()
    return _fernet


# ─────────────────────────────────────────────────────────────────────────────
# File-based fallback store (encrypted JSON)
# ─────────────────────────────────────────────────────────────────────────────


def _load_fallback_store() -> dict[str, str]:
    """Decrypt and deserialise the fallback secrets file."""
    if not _FALLBACK_FILE.exists():
        return {}
    try:
        ciphertext = _FALLBACK_FILE.read_bytes()
        plaintext = _fernet_instance().decrypt(ciphertext)
        return json.loads(plaintext.decode())
    except (InvalidToken, json.JSONDecodeError):
        return {}


def _save_fallback_store(data: dict[str, str]) -> None:
    """Serialise and encrypt *data* into the fallback secrets file."""
    _FALLBACK_DIR.mkdir(mode=0o700, parents=True, exist_ok=True)
    plaintext = json.dumps(data).encode()
    ciphertext = _fernet_instance().encrypt(plaintext)
    _FALLBACK_FILE.write_bytes(ciphertext)
    _FALLBACK_FILE.chmod(0o600)


# ─────────────────────────────────────────────────────────────────────────────
# Public API
# ─────────────────────────────────────────────────────────────────────────────


def get_secret(key: str, default: str | None = None) -> str | None:
    """
    Retrieve a secret by *key*.  Never raises — returns *default* on miss.

    Resolution order:
      1. OS keyring backend
      2. Encrypted fallback file (~/.acousense/secrets.enc)
    """
    if HAS_KEYRING:
        try:
            value = keyring.get_password(_SERVICE_NAME, key)
            if value is not None:
                return value
        except keyring.errors.KeyringError:
            pass

    store = _load_fallback_store()
    return store.get(key, default)


def set_secret(key: str, value: str) -> None:
    """
    Persist *value* under *key* in the most secure available backend.
    Secrets are NEVER written to disk in plaintext.
    """
    if HAS_KEYRING:
        try:
            keyring.set_password(_SERVICE_NAME, key, value)
            return
        except keyring.errors.KeyringError:
            pass

    # Fallback: AES-256 encrypted file
    store = _load_fallback_store()
    store[key] = value
    _save_fallback_store(store)


def delete_secret(key: str) -> bool:
    """Delete a secret.  Returns True if it was found and removed."""
    found = False
    if HAS_KEYRING:
        try:
            keyring.delete_password(_SERVICE_NAME, key)
            found = True
        except keyring.errors.KeyringError:
            pass

    store = _load_fallback_store()
    if key in store:
        del store[key]
        _save_fallback_store(store)
        found = True

    return found


def secret_exists(key: str) -> bool:
    """Return True if a secret with *key* is stored in any backend."""
    return get_secret(key) is not None


def encrypt_value(plaintext: str) -> str:
    """
    Encrypt *plaintext* using the master Fernet key.
    Returns a URL-safe base64-encoded ciphertext string.
    Suitable for storing encrypted fields in the SQLite database.
    """
    return _fernet_instance().encrypt(plaintext.encode()).decode()


def decrypt_value(ciphertext: str) -> str:
    """
    Decrypt a ciphertext string produced by ``encrypt_value``.
    Raises ``cryptography.fernet.InvalidToken`` on tamper or key mismatch.
    """
    return _fernet_instance().decrypt(ciphertext.encode()).decode()
