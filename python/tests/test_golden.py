"""Reproduces test/regress.sh's comparisons against test/golden/, but calling
gwm.Index directly (in-process) instead of shelling out to gwm-build /
gwm-search. %f and Python's :.6f format the same correctly-rounded double
(pybind11 widens the core's float32 similarity losslessly to float64), so
these are expected to match byte-for-byte.
"""

from __future__ import annotations

from pathlib import Path

import gwm

from conftest import DAT, GOLDEN


def _golden_text(name: str) -> str:
    return (GOLDEN / f"{name}.txt").read_text()


def _build_text(index: gwm.Index) -> str:
    return (
        f"length of id list:{index.index_length}\n"
        f"depth of wavelet matrix:{index.matrix_depth}\n"
        f"total memory (byte): {index.memory_bytes}\n"
    )


def _search_text(index: gwm.Index, query_path: Path, threshold: float) -> str:
    hits_per_query = index.search_file(str(query_path), threshold)
    lines = []
    total = 0
    for i, hits in enumerate(hits_per_query):
        lines.append(f"id:{i} " + "".join(f"{gid}:{sim:.6f} " for gid, sim in hits))
        total += len(hits)
    average = total / len(hits_per_query) if hits_per_query else float("nan")
    lines.append(f"average # of outputs:{average:.6f}")
    return "\n".join(lines) + "\n"


def test_build_it2_matches_golden(index_it2: gwm.Index) -> None:
    assert _build_text(index_it2) == _golden_text("build-it2")


def test_build_it3_matches_golden(index_it3: gwm.Index) -> None:
    assert _build_text(index_it3) == _golden_text("build-it3")


def test_search_q08_matches_golden(index_it2: gwm.Index) -> None:
    text = _search_text(index_it2, DAT / "mutagen_query.gsp", 0.8)
    assert text == _golden_text("search-q08")


def test_search_self06_matches_golden(index_it2: gwm.Index) -> None:
    # 680 self-queries: the whole database searched against itself.
    text = _search_text(index_it2, DAT / "mutagen.gsp", 0.6)
    assert text == _golden_text("search-self06")


def test_search_q09it3_matches_golden(index_it3: gwm.Index) -> None:
    text = _search_text(index_it3, DAT / "mutagen_query.gsp", 0.9)
    assert text == _golden_text("search-q09it3")
