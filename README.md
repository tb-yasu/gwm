# gwm

Similarity search over graph databases, in Python or from the command line.

`gwm` represents every graph as the set of subtree patterns found by the
Weisfeiler-Lehman (WL) graph kernel, indexes that feature space with a
[wavelet matrix](https://en.wikipedia.org/wiki/Wavelet_Tree), and answers
threshold queries — "every database graph whose cosine similarity with this
query graph is at least θ" — with a single DFS over the matrix instead of a
linear scan. It has no runtime dependencies.

This is the reference implementation of:

> Yasuo Tabei and Koji Tsuda. **Kernel-based Similarity Search in Massive
> Graph Databases with Wavelet Trees.** *Proceedings of the 2011 SIAM
> International Conference on Data Mining (SDM), pp. 154–165.*
> https://doi.org/10.1137/1.9781611972818.14

The package was previously distributed as **gWT**; `gwm` is the same
algorithm and file format under a new name.

## Installation

```sh
pip install gwm
```

Prebuilt wheels are provided for Linux (x86_64, aarch64) and macOS (x86_64,
arm64) on CPython 3.9–3.14. There are no Windows wheels; other platforms can
build from the source distribution, which needs only a C++17 compiler (see
[Building from source](#building-from-source)).

## Quickstart

Build an index from a database file and search it with a query file, both in
[gSpan format](#graph-file-format):

```python
import gwm

index = gwm.Index.build("db.gsp", iteration=2)
index.save("index.bin")

index = gwm.Index.load("index.bin")
for hits in index.search_file("query.gsp", threshold=0.8):
    print(hits)  # [(graph_id, similarity), ...], one list per query graph
```

Graphs can also be built and searched entirely in memory. Each graph is a
`(labels, edges)` pair: `labels` is a 0-based list of integer vertex labels,
`edges` is a list of `(u, v, edge_label)` triples (also 0-based, undirected):

```python
import gwm

graphs = [
    ([6, 6, 6, 6], [(0, 1, 1), (1, 2, 1), (2, 3, 1), (3, 0, 1)]),
    ([6, 6, 7],    [(0, 1, 1), (1, 2, 2)]),
]

index = gwm.Index.build_from_graphs(graphs, iteration=2)
hits = index.search_graphs(graphs, threshold=0.8)
```

`gwm.from_networkx()` converts a `networkx.Graph` (read from `label` node/edge
attributes by default) into that same `(labels, edges)` form:

```python
import networkx as nx
import gwm

g = nx.Graph()
g.add_node(0, label=6)
g.add_node(1, label=6)
g.add_edge(0, 1, label=1)

index = gwm.Index.build_from_graphs([gwm.from_networkx(g)], iteration=2)
```

networkx itself is not a dependency — `from_networkx()` only relies on the
standard `.nodes(data=True)` / `.edges(data=True)` protocol.

### Index reference

- `Index.build(path, iteration=2)`, `Index.build_from_graphs(graphs, iteration=2)` — construct.
- `Index.load(path)` / `index.save(path)` — the two sides of the on-disk index format.
- `index.search_file(path, threshold)`, `index.search_graphs(graphs, threshold)` — encode queries with the index's own label dictionary and search in one call.
- `index.encode_query_file(path)`, `index.encode_query_graphs(graphs)` — encode only, returning each query graph's WL label set (`list[list[int]]`) for reuse with `search_encoded`.
- `index.search_encoded(labels, threshold)` — search a single already-encoded query.
- `index.num_graphs`, `index.iterations`, `index.matrix_depth`, `index.index_length`, `index.memory_bytes` — index statistics.

`threshold` must be in `(0, 1]`; it is the cosine-similarity cutoff, not a
count.

## Command line

The pip package installs a `gwm` command:

```sh
# build an index (-iteration = number of WL relabeling rounds, default 2)
gwm build -iteration 2 db.gsp index.bin

# search (-kthreshold = cosine similarity threshold, default 0.8)
gwm search -kthreshold 0.8 index.bin query.gsp
```

Building from source (see below) also produces standalone, dependency-free
`gwm-build` / `gwm-search` binaries with the same options; these are not part
of the pip package.

## Graph file format

Both the CLI and `Index.build`/`search_file` read databases and queries in
gSpan format — one record per graph, separated by a blank line:

```
t # <graph-id> <class-label> <name>
v <vertex-id> <vertex-label>
e <from> <to> <edge-label>
```

Vertex ids are 1-based and must be declared with `v` before they appear in an
`e` line. `gwm.read_gspan(path)` parses this format into the same
`(labels, edges)` list used by the in-memory API.

## How it works

For each graph, every WL relabeling round produces one signature string per
vertex (its own label, then its sorted `(neighbor label, edge label)`
multiset); each distinct signature is assigned a dense integer code shared
across the whole database. A graph's feature set is the deduplicated set of
codes seen across all rounds, and its norm is the size of that set.

At build time, every code's posting list (the graphs containing it) is
concatenated into one sequence and stored as a wavelet matrix. At query time,
each of the query's codes contributes its interval in that sequence; a DFS
over the matrix intersects those intervals level by level, pruning any
subtree whose surviving range is already too small to reach the threshold.
At a leaf, the surviving range size is `|query ∩ graph|`, giving the cosine
similarity `|query ∩ graph| / sqrt(|query| · |graph|)` directly — no
per-graph scan required.

## Building from source

```sh
make                  # from the repo root; produces src/gwm-build and src/gwm-search
make test             # build + output-parity regression
```

or with CMake:

```sh
cmake -S . -B build && cmake --build build
```

Both require only a C++17 compiler; there are no external dependencies.

## License

gwm's own code is MIT licensed (see [LICENSE](LICENSE)). It vendors
Sebastiano Vigna's [sux](https://sux.di.unimi.it/) rank/select library
(`src/rank9sel.{h,cpp}`, `src/macros.h`, `src/popcount.h`), which is
LGPL-2.1-or-later (see [LICENSES/LGPL-2.1-or-later.txt](LICENSES/LGPL-2.1-or-later.txt)).
The distribution as a whole is therefore `MIT AND LGPL-2.1-or-later`; full
source for every component, including the vendored library, ships in the
source distribution.
