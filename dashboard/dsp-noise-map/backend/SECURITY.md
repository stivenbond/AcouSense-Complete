# Security & Key Management

AcouSense strictly offloads keys outside of source logic.
Keyring Fallback Priority Chain (`src/acousense/utils/secrets.py`):
1. **System Keyring:** `keyring get acousense sqlcipher-passphrase / api-key`
2. **Environment Variable:** `ACOUSENSE_SQLCIPHER_PASSPHRASE` / `ACOUSENSE_API_KEY`
3. **Local File:** `~/.config/acousense/secrets.json` -> 600 perm

Do NOT commit vulnerabilities or API keys. If you find one, email `security@acousense` and DO NOT publicize it!
