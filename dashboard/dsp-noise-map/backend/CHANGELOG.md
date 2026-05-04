# Changelog
## [2.0.0] - 2025 Refactor
- Introduced `uv` core dependencies.
- Replaced bare SQL access with rigorous `aiosqlite` and `alembic`.
- Extracted logic to `services/` and `CommandHistory`.
- Enabled Terminal TUI with Textual.
- Hardened Kubernetes and Docker deployments.
