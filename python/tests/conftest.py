from __future__ import annotations

from pathlib import Path

import pytest

import gwm

# python/tests/conftest.py -> python/tests -> python -> repo root
REPO_ROOT = Path(__file__).resolve().parents[2]
DAT = REPO_ROOT / "dat"
GOLDEN = REPO_ROOT / "test" / "golden"
SRC = REPO_ROOT / "src"

DB = DAT / "mutagen.gsp"
QUERY = DAT / "mutagen_query.gsp"


@pytest.fixture(scope="session")
def index_it2() -> gwm.Index:
    return gwm.Index.build(str(DB), iteration=2)


@pytest.fixture(scope="session")
def index_it3() -> gwm.Index:
    return gwm.Index.build(str(DB), iteration=3)
