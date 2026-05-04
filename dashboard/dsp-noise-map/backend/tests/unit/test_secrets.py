import pytest
from acousense.utils.secrets import get_secret


def test_get_secret_env(monkeypatch):
    monkeypatch.setenv("ACOUSENSE_TEST_SECRET", "12345")
    val = get_secret("acousense", "test-secret")
    assert val == "12345"


def test_get_secret_raises(monkeypatch):
    monkeypatch.delenv("ACOUSENSE_TEST_SECRET", raising=False)
    # mock keyring to raise
    import keyring

    monkeypatch.setattr(keyring, "get_password", lambda s, k: None)
    with pytest.raises(RuntimeError):
        get_secret("acousense", "test-secret")
