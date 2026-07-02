"""`python -m gwm build|search` subprocess output vs test/golden/: the same
comparison test/regress.sh makes for the native CLI. The Python CLI never
prints a line containing "time", so the grep -v "time" regress.sh applies to
the native binaries' output is a no-op here -- kept anyway so both suites
compare golden the same way.
"""

from __future__ import annotations

import subprocess
import sys

from conftest import DB, GOLDEN, QUERY


def _run_cli(*args: str) -> str:
    result = subprocess.run(
        [sys.executable, "-m", "gwm", *args],
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout


def _filtered(text: str) -> str:
    return "\n".join(line for line in text.splitlines() if "time" not in line) + "\n"


def test_cli_build_it2_matches_golden(tmp_path) -> None:
    out = _run_cli("build", "-iteration", "2", str(DB), str(tmp_path / "idx2.bin"))
    assert _filtered(out) == (GOLDEN / "build-it2.txt").read_text()


def test_cli_build_it3_matches_golden(tmp_path) -> None:
    out = _run_cli("build", "-iteration", "3", str(DB), str(tmp_path / "idx3.bin"))
    assert _filtered(out) == (GOLDEN / "build-it3.txt").read_text()


def test_cli_search_q08_matches_golden(tmp_path) -> None:
    index_path = tmp_path / "idx2.bin"
    _run_cli("build", "-iteration", "2", str(DB), str(index_path))
    out = _run_cli("search", "-kthreshold", "0.8", str(index_path), str(QUERY))
    assert _filtered(out) == (GOLDEN / "search-q08.txt").read_text()
