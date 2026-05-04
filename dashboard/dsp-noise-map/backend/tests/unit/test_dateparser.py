from datetime import datetime

import pytest
import typer
from acousense.cli.app import parse_flexible_date


def test_parse_flexible_date():
    scenarios = [
        "2025-01-01",
        "3 days ago",
        "last Monday",
        "next Friday 9am",
        "1735689600",
    ]
    for sc in scenarios:
        dt = parse_flexible_date(sc)
        assert isinstance(dt, datetime)

    with pytest.raises(typer.BadParameter):
        parse_flexible_date("this is not a date")
