"""gwm: similarity search over graph databases using the Weisfeiler-Lehman
(WL) kernel and a wavelet matrix.

>>> index = Index.build("db.gsp", iteration=2)
>>> index.search_file("query.gsp", threshold=0.8)
[[(3, 1.0), (17, 0.86)], []]
"""

from __future__ import annotations

from typing import Dict, Iterable, List, Sequence, Tuple

from . import _core
from ._core import __version__

__all__ = [
    "Index",
    "GraphTuple",
    "from_networkx",
    "read_gspan",
    "__version__",
]

# One in-memory graph: 0-based vertex labels, and edges as 0-based
# (u, v, edge_label) triples. This is what build_from_graphs/search_graphs/
# encode_query_graphs and from_networkx/read_gspan produce and consume.
GraphTuple = Tuple[Sequence[int], Sequence[Tuple[int, int, int]]]

Hit = Tuple[int, float]


def _check_threshold(threshold: float) -> None:
    if not (0.0 < threshold <= 1.0):
        raise ValueError(f"threshold must be in (0, 1], got {threshold!r}")


class Index:
    """A built or loaded gwm similarity index.

    Do not construct directly; use `Index.build`, `Index.build_from_graphs`,
    or `Index.load`.
    """

    def __init__(self, core: "_core.Index") -> None:
        self._core = core

    @classmethod
    def build(cls, path: str, iteration: int = 2) -> "Index":
        """Builds an index from a gSpan database file."""
        core = _core.Index()
        core.build(str(path), iteration)
        return cls(core)

    @classmethod
    def build_from_graphs(cls, graphs: Iterable[GraphTuple], iteration: int = 2) -> "Index":
        """Builds an index from in-memory (labels, edges) graphs."""
        graph_list = list(graphs)
        if not graph_list:
            raise ValueError("graphs must be non-empty")
        core = _core.Index()
        core.build_from_graphs(graph_list, iteration)
        return cls(core)

    @classmethod
    def load(cls, path: str) -> "Index":
        """Loads a previously saved index."""
        core = _core.Index()
        core.load(str(path))
        return cls(core)

    def save(self, path: str) -> None:
        """Saves the index in gwm's native binary format."""
        self._core.save(str(path))

    def encode_query_file(self, path: str) -> List[List[int]]:
        """Encodes each graph in a gSpan file into this index's WL label
        codes, without searching. Unknown labels are silently dropped, same
        as search_file/search_graphs."""
        return self._core.encode_query_file(str(path))

    def encode_query_graphs(self, graphs: Iterable[GraphTuple]) -> List[List[int]]:
        """Like encode_query_file, for in-memory (labels, edges) graphs."""
        return self._core.encode_query_graphs(list(graphs))

    def search_encoded(self, labels: Sequence[int], threshold: float) -> List[Hit]:
        """Searches a single already-encoded query (see encode_query_file/
        encode_query_graphs). Returns (graph_id, similarity) hits."""
        _check_threshold(threshold)
        return self._core.search_encoded(list(labels), threshold)

    def search_file(self, path: str, threshold: float) -> List[List[Hit]]:
        """Encodes and searches every graph in a gSpan query file. Returns
        one (graph_id, similarity) hit list per query graph, in file order."""
        _check_threshold(threshold)
        encoded = self._core.encode_query_file(str(path))
        return [self._core.search_encoded(q, threshold) for q in encoded]

    def search_graphs(self, graphs: Iterable[GraphTuple], threshold: float) -> List[List[Hit]]:
        """Like search_file, for in-memory (labels, edges) graphs."""
        _check_threshold(threshold)
        encoded = self._core.encode_query_graphs(list(graphs))
        return [self._core.search_encoded(q, threshold) for q in encoded]

    @property
    def num_graphs(self) -> int:
        """Number of graphs in the database."""
        return self._core.num_graphs

    @property
    def iterations(self) -> int:
        """Number of WL relabeling rounds the index was built with."""
        return self._core.iterations

    @property
    def matrix_depth(self) -> int:
        """Depth of the wavelet matrix."""
        return self._core.matrix_depth

    @property
    def index_length(self) -> int:
        """Length of the concatenated posting-list sequence backing the
        wavelet matrix (sum of every graph's deduplicated WL feature-set
        size, across all rounds)."""
        return self._core.index_length

    @property
    def memory_bytes(self) -> int:
        """Approximate in-memory size of the index, in bytes."""
        return self._core.memory_bytes


def from_networkx(g, node_label: str = "label", edge_label: str = "label") -> GraphTuple:
    """Converts a networkx.Graph into gwm's (labels, edges) form.

    Vertex order follows g.nodes(); node/edge label attributes default to 0
    when absent. networkx itself is not imported here -- any object
    implementing the same .nodes(data=True)/.edges(data=True) protocol
    works.
    """
    nodes = list(g.nodes())
    index_of: Dict[object, int] = {n: i for i, n in enumerate(nodes)}
    labels = [int(g.nodes[n].get(node_label, 0)) for n in nodes]
    edges = [
        (index_of[u], index_of[v], int(data.get(edge_label, 0)))
        for u, v, data in g.edges(data=True)
    ]
    return labels, edges


def read_gspan(path: str) -> List[GraphTuple]:
    """Pure-Python gSpan reader, producing the same (labels, edges) form as
    from_networkx().

    Mirrors GRAPHKERNEL::Graph::read (src/Graph.cpp): a "t" line starts a
    record; a blank line or the next "t" line ends it. Vertex ids are
    1-based and need not be contiguous -- gaps default to label 0, matching
    the C++ reader's dynamic resize-on-demand.
    """
    graphs: List[GraphTuple] = []
    vlabels: Dict[int, int] = {}
    edges: List[Tuple[int, int, int]] = []
    started = False

    def flush() -> None:
        if not started:
            return
        n = max(vlabels) if vlabels else 0
        labels = [vlabels.get(i + 1, 0) for i in range(n)]
        graphs.append((labels, [(u - 1, v - 1, w) for u, v, w in edges]))

    with open(path) as f:
        for raw_line in f:
            line = raw_line.strip()
            if not line:
                flush()
                vlabels, edges, started = {}, [], False
                continue
            tok = line.split()
            if tok[0] == "t":
                flush()
                vlabels, edges, started = {}, [], True
            elif tok[0] == "v":
                vlabels[int(tok[1])] = int(tok[2])
            elif tok[0] == "e":
                edges.append((int(tok[1]), int(tok[2]), int(tok[3])))
    flush()
    return graphs
