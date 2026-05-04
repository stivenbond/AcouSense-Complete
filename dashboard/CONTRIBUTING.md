# Contributing to AcouSense

We're thrilled you want to help improve AcouSense! We welcome contributions ranging from bug fixes and documentation improvements to large architectural changes.

## 🛠 Development Workflow (2025 Standards)

The backend now uses a modern `src/` layout driven by `pyproject.toml`.
We strongly enforce code formatting and static typing.

### 1. Setup Your Environment

```bash
cd dsp-noise-map/backend
python -m venv .venv
source .venv/bin/activate
pip install -e ".[dev]"
```

### 2. Pre-Commit Hooks
We use `pre-commit` to prevent badly formatted or incorrectly typed code from reaching `main`.
```bash
pre-commit install
```
This ensures `black` and `mypy` run locally before every commit.

### 3. Submitting PRs
1. **Fork and Branch**: `git checkout -b feature/your-awesome-feature`.
2. **Implement**: Keep your logic compartmentalized. If contributing to the spatial algorithm, work in `src/core/engine.py`.
3. **CLI Testing**: Use the new Typer CLI to test commands: `acousense serve` or `acousense tui`.
4. **Push and Open**: Open a PR. Include screenshots of the Textual TUI if you've altered system metrics.

## 🤝 Code of Conduct
Please be respectful and patient with reviewers. We're all here to build an incredible open-source ecosystem.
