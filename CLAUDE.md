# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

gWM — similarity search over graph databases using the Weisfeiler-Lehman (WL) subtree kernel and a wavelet matrix. Each graph is represented as its set of WL label codes; `gwm-search` returns every database graph whose cosine similarity with a query graph (in that feature space) is at least a threshold. C++17, no external dependencies. `pip install gwm` gives the same index-build/search functionality from Python (see [Python package](#python-package)) around the same core library.

## Build

```sh
make                  # from the repo root (delegates to src/); produces src/gwm-build and src/gwm-search
make test             # build + output-parity regression
make bench            # build + 34k-graph benchmark
make clean && make debug   # -g build (clean first: debug reuses stale -O3 objects otherwise)
```

CMake also works from the repo root: `cmake -S . -B build && cmake --build build`.

## Run

```sh
# build an index (-iteration = number of WL relabeling rounds, default 2)
src/gwm-build -iteration 2 dat/mutagen.gsp /tmp/idx

# search (-kthreshold = cosine similarity threshold, default 0.8)
src/gwm-search -kthreshold 0.8 /tmp/idx dat/mutagen_query.gsp
```

Per query graph the output line is `id:<query> <hitid>:<sim> ...`, followed by timing stats. `dat/` contains gSpan-format molecular datasets (mutagen.gsp: 680 graphs; mutagen_query.gsp: 10 graphs; the CA*/CM*/CI* files are NCI screening sets).

## Testing / verification

```sh
test/regress.sh            # output-parity regression (must print OK for all cases)
test/regress.sh --update   # regenerate test/golden/ after an INTENTIONAL output change
test/bench.sh              # 34k-graph benchmark (50x mutagen); set BENCH_DIR to reuse the data
```

`regress.sh` builds the binaries, runs gwm-build (iteration 2 and 3) and three searches on `dat/mutagen*`, and diffs stdout against `test/golden/`, excluding lines containing "time". Hit lists, similarity values, "length of id list", "depth of wavelet matrix", and "total memory" must stay byte-identical unless the change intentionally alters results. Run it after every behavioral change; run `bench.sh` before/after performance work.

## Python package

`pyproject.toml` + `python/` build a `gwm` PyPI package (pybind11 + scikit-build-core) around the same `gwm_core` library the CLI links against. It's off by default for `make`/plain `cmake`; only `pip install` (or `cmake -DGWM_BUILD_PYTHON=ON`) builds it.

```sh
pip install .          # builds gwm._core (pybind11) and installs python/gwm
pytest python/tests    # gate: must be all green after any change under src/ or python/
```

- `src/gWM.hpp`/`gWM.cpp`: the in-process API the bindings call (`buildIndex`/`buildIndexFromGraphs`/`saveIndex`/`loadIndex`/`encodeQueryFile`/`encodeQueryGraphs`/`searchEncoded` + accessors). Throws `std::runtime_error`/`std::invalid_argument` instead of the CLI's `cerr` + `exit(1)`, and never touches stdout/stderr.
- `python/bindings.cpp`: the pybind11 module (`gwm._core.Index`), wrapping one `gWM` instance behind a `std::mutex` so an `Index` is safe to call from multiple Python threads. Every method is bound with `py::call_guard<py::gil_scoped_release>` — the GIL is released *before* the method body runs, which then locks the mutex; never lock the mutex first, or two threads can deadlock on GIL-vs-mutex. pybind11 3.x's `def_property_readonly` rejects `call_guard` passed directly (compile-time `static_assert`); wrap the getter in `py::cpp_function` first, as its error message says to.
- `python/gwm/`: the pure-Python layer — `Index` (wraps `_core.Index`, adds threshold validation), `from_networkx()`, `read_gspan()` (a pure-Python gSpan reader mirroring `Graph::read`, used for in-memory/file-path equivalence tests), and the `gwm` console script (`gwm build|search`, reproducing `gwm-build`/`gwm-search`'s stdout format).

`test/golden/` is the shared source of truth for both suites: `python/tests/test_golden.py` reconstructs the same lines in-process (`f"id:{i} " + f"{gid}:{sim:.6f} "` — `%f` and `:.6f` format the same correctly-rounded double), and `test_cli.py` runs `python -m gwm` as a subprocess; both must match `test/golden/` byte-for-byte, same as `test/regress.sh`. See `PUBLISHING.md` for the release process.

## Architecture

Two binaries share one core library (Graph.cpp, WLKernel.cpp, gWM.cpp, rank9sel.cpp):

- `Build.cpp` → `gWM::constructor`: read DB → WL relabeling → inverted index → wavelet matrix → save index file.
- `Search.cpp` → `gWM::searcher`: load index → encode query graphs with the same label dictionary → range search per query.

Data flow:

1. `WLKernel` (namespace GRAPHKERNEL) parses gSpan files (`t`/`v`/`e` lines; vertex ids are 1-based, so vector slot 0 of a `Graph` is unused) and runs WL iterations. Each distinct signature string (`iter_label` initially, then `iter_ownlabel(_neighborlabel_elabel)*` with sorted neighbor pairs) maps to a dense integer code via the `s2c` dictionary.
2. `gWM::buildInvertedIndex`: after every WL iteration, each graph's deduplicated label set is appended to `invertedIndex[label]`, and `vnums[id]` accumulates the graph's feature-set size |G| (the cosine norm).
3. Posting lists are concatenated into one graph-id sequence; `intervalIndex[label]` stores each label's [s,e] interval in it; the sequence is stored as a wavelet matrix (`bits` per level + one rank9sel per level + `zeros` array).
4. `gWM::rangeSearch` runs a DFS over the wavelet matrix. Each query label contributes its interval; at every level intervals split into 0/1 sides via rank. At a leaf (= graph id) the number of surviving intervals equals |Q∩G|, and similarity = |ranges| / √(|Q|·|G|). Subtrees with ranges.size() < θ²·|Q| are pruned.

Invariants that are easy to break:

- Build and search must share the exact `s2c` dictionary: `WLKernel`'s copy constructor copies `s2c`/`counter` (not the graphs) so queries are encoded with database codes, and the dictionary is serialized inside the index file.
- Label codes are dense and assigned in encounter order (graphs in file order). Changing iteration order changes index bytes but must not change search results.
- The index file is a raw little-endian binary dump with fixed-width sizes (uint64/uint32). Any change to `save` needs the matching `load` change and breaks previously built index files — say so explicitly when it happens.
- stdout is consumed by experiment scripts: keep the `id:%zu ` / `%u:%f ` line format and the stat lines stable.
- `buildIndexCore()` (used by the in-process `buildIndex()`/`buildIndexFromGraphs()`) duplicates `constructor()`'s pipeline rather than sharing it — `constructor()` itself is intentionally untouched. `test/regress.sh` exercises `constructor()`, not `buildIndexCore()`, so a pipeline change (new step, different order) needs updating both by hand; `python/tests` (or `cmp`-ing a Python-built index against a CLI-built one) is what catches drift between them. `encodeQueries()`, by contrast, is a real shared extraction — both `searcher()` and `encodeQueryFile()`/`encodeQueryGraphs()` call it, so `regress.sh` does cover it.

## Vendored code

`rank9sel.{h,cpp}`, `macros.h`, `popcount.h` are Sebastiano Vigna's sux library (LGPL): broadword constant-time rank/select. Keep edits there minimal (compiler compatibility only); their warning suppressions live in the Makefile / CMakeLists.txt, not in the source.
