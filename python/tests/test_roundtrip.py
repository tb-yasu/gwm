"""save()/load() roundtrip, and reloading a *different* index into an
already-used Index instance.

The latter exercises gWM::loadIndex's graphKernel.initialize() call: WLKernel
::load() does not clear the label dictionary itself (it only inserts into
it), so an instance that already built or loaded one index must reset its
dictionary before loading another -- otherwise stale (string -> code)
entries from the first index could leak into query encoding done against
the second. That reset is only observable by loading twice into the same
core object, so this test reaches into Index._core (the low-level pybind11
wrapper) rather than going through the classmethod-only public Index.load().
"""

from __future__ import annotations

import gwm

from conftest import QUERY


def test_save_load_roundtrip(index_it2: gwm.Index, tmp_path) -> None:
    path = tmp_path / "index.bin"
    index_it2.save(str(path))

    loaded = gwm.Index.load(str(path))
    assert loaded.num_graphs == index_it2.num_graphs
    assert loaded.iterations == index_it2.iterations
    assert loaded.matrix_depth == index_it2.matrix_depth
    assert loaded.index_length == index_it2.index_length
    assert loaded.memory_bytes == index_it2.memory_bytes
    assert loaded.search_file(str(QUERY), 0.8) == index_it2.search_file(str(QUERY), 0.8)


def test_reload_on_existing_instance_does_not_leak_dictionary(
    index_it2: gwm.Index, index_it3: gwm.Index, tmp_path
) -> None:
    path2 = tmp_path / "idx2.bin"
    path3 = tmp_path / "idx3.bin"
    index_it2.save(str(path2))
    index_it3.save(str(path3))

    reused = gwm.Index.load(str(path2))
    assert reused.search_file(str(QUERY), 0.8) == index_it2.search_file(str(QUERY), 0.8)

    # Load a *different* index (different iteration count, hence a
    # different label dictionary) into the same core object.
    reused._core.load(str(path3))
    assert reused.iterations == index_it3.iterations
    assert reused.search_file(str(QUERY), 0.9) == index_it3.search_file(str(QUERY), 0.9)
