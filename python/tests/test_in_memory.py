"""In-memory (labels, edges) graphs, produced by read_gspan(), must give
results identical to reading the same gSpan files directly -- this is what
actually pins down that the 0-based-to-1-based conversion in bindings.cpp
(toGraph) matches GRAPHKERNEL::Graph::read.
"""

from __future__ import annotations

import gwm

from conftest import DB, QUERY


def test_read_gspan_build_matches_file_build(index_it2: gwm.Index) -> None:
    graphs = gwm.read_gspan(str(DB))
    mem_index = gwm.Index.build_from_graphs(graphs, iteration=2)

    assert mem_index.num_graphs == index_it2.num_graphs
    assert mem_index.index_length == index_it2.index_length
    assert mem_index.matrix_depth == index_it2.matrix_depth
    assert mem_index.search_file(str(QUERY), 0.8) == index_it2.search_file(str(QUERY), 0.8)


def test_read_gspan_encode_matches_file_encode(index_it2: gwm.Index) -> None:
    query_graphs = gwm.read_gspan(str(QUERY))
    assert index_it2.encode_query_graphs(query_graphs) == index_it2.encode_query_file(str(QUERY))


def test_search_graphs_matches_search_file(index_it2: gwm.Index) -> None:
    query_graphs = gwm.read_gspan(str(QUERY))
    assert index_it2.search_graphs(query_graphs, 0.8) == index_it2.search_file(str(QUERY), 0.8)
