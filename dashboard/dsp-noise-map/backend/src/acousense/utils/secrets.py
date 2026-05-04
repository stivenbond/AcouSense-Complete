import json
import os
from pathlib import Path

import keyring


def get_secret(service: str, key: str) -> str:
    """Retrieve secret using a fallback chain: keyring -> env var -> file -> raise."""
    try:
        secret = keyring.get_password(service, key)
        if secret:
            return secret
    except Exception:
        pass

    env_vars = [key, f"{service.upper()}_{key.upper()}".replace("-", "_")]
    for env_key in env_vars:
        if env_key in os.environ:
            return os.environ[env_key]

    app_dir = Path.home() / ".config" / service
    secrets_file = app_dir / "secrets.json"
    if secrets_file.exists():
        try:
            with open(secrets_file) as f:
                secrets = json.load(f)
                if key in secrets:
                    return secrets[key]
        except Exception:
            pass

    raise RuntimeError(
        f"Secret '{key}' for service '{service}' could not be found.\n"
        "Please securely store it using one of the following methods:\n"
        f"1. Keyring: `keyring set {service} {key}`\n"
        f"2. Environment Variable: `export {key}=<secret>`\n"
        f"3. Secrets File: `~/.config/{service}/secrets.json` (0600 permissions)"
    )
