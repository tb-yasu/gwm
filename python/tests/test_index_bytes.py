"""Byte-identical index files between the native CLI (src/gwm-build) and the
Python build path.

Local-only: skipped unless the CLI binary has already been built (e.g. via
`make`), since scikit-build-core's out-of-tree CMake build for the wheel
never touches src/. Also inherently platform-local, not just
environment-local -- label codes are assigned in std::unordered_map
encounter order, which is deterministic within one platform/build but not
guaranteed to match across platforms/toolchains.
"""

from __future__ import annotations

import subprocess

import pytest

import gwm

from conftest import DB, SRC

GWM_BUILD = SRC / "gwm-build"


@pytest.mark.skipif(not GWM_BUILD.exists(), reason="src/gwm-build not built (run `make` first)")
def test_index_bytes_match_native_cli(tmp_path) -> None:
    cli_index = tmp_path / "cli.idx"
    subprocess.run(
        [str(GWM_BUILD), "-iteration", "2", str(DB), str(cli_index)],
        check=True,
        capture_output=True,
    )

    py_index = tmp_path / "py.idx"
    gwm.Index.build(str(DB), iteration=2).save(str(py_index))

    assert cli_index.read_bytes() == py_index.read_bytes()
