"""Error paths: the in-process API raises Python exceptions instead of the
CLI entry points' cerr + exit(1) (std::runtime_error -> RuntimeError,
std::invalid_argument -> ValueError, both via pybind11's automatic
translation).
"""

from __future__ import annotations

import pytest

import gwm

from conftest import DAT, QUERY


def test_build_missing_file_raises() -> None:
    with pytest.raises(RuntimeError):
        gwm.Index.build(str(DAT / "does-not-exist.gsp"), iteration=2)


def test_load_missing_file_raises() -> None:
    with pytest.raises(RuntimeError):
        gwm.Index.load(str(DAT / "does-not-exist.bin"))


def test_encode_query_missing_file_raises(index_it2: gwm.Index) -> None:
    with pytest.raises(RuntimeError):
        index_it2.encode_query_file(str(DAT / "does-not-exist.gsp"))


@pytest.mark.parametrize("threshold", [0.0, -0.1, 1.5])
def test_search_rejects_invalid_threshold(index_it2: gwm.Index, threshold: float) -> None:
    with pytest.raises(ValueError):
        index_it2.search_file(str(QUERY), threshold)


def test_search_encoded_rejects_invalid_threshold(index_it2: gwm.Index) -> None:
    with pytest.raises(ValueError):
        index_it2.search_encoded([0], 0.0)


def test_build_from_graphs_rejects_edge_endpoint_out_of_range() -> None:
    with pytest.raises(ValueError):
        gwm.Index.build_from_graphs([([6, 6], [(0, 5, 1)])], iteration=2)


def test_encode_query_graphs_rejects_edge_endpoint_out_of_range(index_it2: gwm.Index) -> None:
    with pytest.raises(ValueError):
        index_it2.encode_query_graphs([([6, 6], [(0, 5, 1)])])


def test_build_from_graphs_rejects_empty_database() -> None:
    with pytest.raises(ValueError):
        gwm.Index.build_from_graphs([], iteration=2)
