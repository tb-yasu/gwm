# gWM

`gWM` finds every graph in a database whose similarity to a query graph is
at least a given threshold — it answers *threshold* queries, not top-k or
approximate nearest-neighbor search.

It encodes each graph as a binary set of Weisfeiler-Lehman (WL) subtree
features and indexes that feature space with a
[wavelet matrix](https://en.wikipedia.org/wiki/Wavelet_Tree). Queries are
answered by traversing the wavelet matrix rather than explicitly scoring
every database graph. It has no Python runtime dependencies.

`gWM` is the reference implementation of Tabei and Tsuda's graph-kernel
similarity search method (SDM 2011; full citation [below](#citation)). The
package was previously distributed as **gWT**; `gWM` is the same algorithm
and input format under a new name (the on-disk index format is an internal
implementation detail and isn't guaranteed to match gWT's).

## Installation

```sh
pip install gwm
```

Prebuilt wheels are provided for Linux (x86_64, aarch64), macOS (x86_64,
arm64), and Windows (x86_64) on CPython 3.9–3.14. Other platforms may be
able to build from the source distribution, which needs only a C++17
compiler (see [Building from source](#building-from-source)).

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

`graph_id` is always the graph's 0-based position in the database (file
order, or list order for in-memory graphs) — *not* the id written on a
gSpan file's `t #` line, which `gWM` ignores (see
[Graph file format](#graph-file-format)).

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
- `index.encode_query_file(path)`, `index.encode_query_graphs(graphs)` — encode only, returning each query graph's WL code set (`list[list[int]]`) for reuse with `search_encoded`.
- `index.search_encoded(labels, threshold)` — search a single already-encoded query, where `labels` is one WL code set returned by `encode_query_file`/`encode_query_graphs`.
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

The single-dash `-iteration` / `-kthreshold` flags (not `--iteration`) match
the legacy gWT CLI. Building from source (see below) also produces
standalone `gwm-build` / `gwm-search` binaries with no external library
dependencies and the same options; these are not part of the pip package.

## Graph file format

Both the CLI and `Index.build`/`search_file` read databases and queries in
gSpan format — one record per graph, starting with a `t #` line; blank
lines between records are optional (a new `t #` line is itself enough to
end the previous record):

```
t # <anything ignored by gWM>
v <vertex-id> <vertex-label>
e <from> <to> <edge-label>
```

The tokens after `t #` (conventionally a graph id, a class label, and a
name) are ignored by the index — as noted above, a graph's `graph_id` in
results is its 0-based position in the file, not this field. Vertex ids
are 1-based positive integers and must be declared with `v` before they
appear in an `e` line; vertex and edge labels must be non-negative
integers. Vertex ids should be contiguous starting at 1 — gaps are not
rejected, but are silently filled with extra zero-label, edgeless vertices
that then contribute their own WL features. Graphs are treated as
undirected, and self-loops and repeated
edges are read as given (not rejected or merged). `gwm.read_gspan(path)`
parses this format into a list of `(labels, edges)` graphs used by the
in-memory API.

## How it works

For each graph, the initial vertex labels are used as round-0 features;
each subsequent WL relabeling round then produces one signature string per
vertex (its own label, then its sorted `(neighbor label, edge label)`
multiset). Each distinct signature is assigned a dense integer code shared
across the whole database. A graph's feature set is the deduplicated set of
codes seen across all rounds. Unlike the counted feature vectors some WL
subtree-kernel implementations use, this is a *binary* feature vector — a
code is either in a graph's set or it isn't — so a feature set's squared L2
norm is simply its size.

`iteration` (default 2) is the number of WL relabeling rounds run *after*
the initial vertex labels, so `iteration=2` contributes three rounds of
features per graph: the initial labels, plus two rounds of relabeling.

At build time, every code's posting list (the graphs containing it) is
concatenated into one sequence and stored as a wavelet matrix. At query time,
each of the query's codes contributes its interval in that sequence; a DFS
over the matrix intersects those intervals level by level, pruning any
subtree whose surviving range is already too small to reach the threshold.
At a leaf, the surviving range size is the number of codes shared between
the query's feature set `Q` and the graph's feature set `G`, giving the
cosine similarity between them directly: `|Q ∩ G| / sqrt(|Q| · |G|)`,
without explicitly scoring every database graph one by one.

## Limitations

- Graphs are treated as undirected; edges are stored symmetrically
  regardless of a gSpan file's `from`/`to` order.
- Similarity is computed over binary WL subtree-feature sets (present or
  absent), not per-code counts.
- `gWM` supports threshold search, not top-k or approximate
  nearest-neighbor search.
- The on-disk index format is an internal detail and isn't guaranteed to be
  stable across `gWM` versions or compatible with indexes produced by the
  original gWT package.

## When to use

`gWM` is useful when you have a mostly-static graph database and need to
run many threshold-similarity queries against it. For a one-off query
against a small database, a plain linear scan may be simpler and just as
fast.

## Building from source

```sh
make                  # from the repo root; produces src/gwm-build and src/gwm-search
make test             # build + output-parity regression
```

or with CMake:

```sh
cmake -S . -B build && cmake --build build
```

Both require only a C++17 compiler; all non-standard code the build uses is
vendored in the repository (see [License](#license)).

## Citation

If you use `gWM` in published work, please cite:

> Yasuo Tabei and Koji Tsuda. **Kernel-based Similarity Search in Massive
> Graph Databases with Wavelet Trees.** *Proceedings of the 2011 SIAM
> International Conference on Data Mining (SDM), pp. 154–163.*
> https://doi.org/10.1137/1.9781611972818.14

```bibtex
@inproceedings{tabei2011kernel,
  author    = {Yasuo Tabei and Koji Tsuda},
  title     = {Kernel-based Similarity Search in Massive Graph Databases with Wavelet Trees},
  booktitle = {Proceedings of the Eleventh SIAM International Conference on Data Mining (SDM)},
  pages     = {154--163},
  publisher = {SIAM},
  year      = {2011},
  doi       = {10.1137/1.9781611972818.14},
}
```

## License

gWM's own code is MIT licensed (see [LICENSE](LICENSE)). It vendors
Sebastiano Vigna's [sux](https://sux.di.unimi.it/) rank/select library
(`src/rank9sel.{h,cpp}`, `src/macros.h`, `src/popcount.h`), which is
LGPL-2.1-or-later (see [LICENSES/LGPL-2.1-or-later.txt](LICENSES/LGPL-2.1-or-later.txt)).
The distribution as a whole is therefore `MIT AND LGPL-2.1-or-later`; full
source for every component, including the vendored library, ships in the
source distribution.
