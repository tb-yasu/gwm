"""`gwm build` / `gwm search`: reproduce the native gwm-build / gwm-search
binaries' stdout format (see test/golden/ in the source repository) so
scripts can use either interchangeably.
"""

from __future__ import annotations

import argparse
import sys
from typing import List, Optional

from . import Index

_DEFAULT_ITERATION = 2
_DEFAULT_THRESHOLD = 0.8


def _build(args: argparse.Namespace) -> None:
    index = Index.build(args.database, iteration=args.iteration)
    print(f"length of id list:{index.index_length}")
    print(f"depth of wavelet matrix:{index.matrix_depth}")
    index.save(args.indexfile)
    print(f"total memory (byte): {index.memory_bytes}")


def _search(args: argparse.Namespace) -> None:
    index = Index.load(args.indexfile)
    all_hits = index.search_file(args.queryfile, threshold=args.kthreshold)

    total = 0
    for i, hits in enumerate(all_hits):
        line = f"id:{i} " + "".join(f"{gid}:{sim:.6f} " for gid, sim in hits)
        print(line)
        total += len(hits)

    average = total / len(all_hits) if all_hits else float("nan")
    print(f"average # of outputs:{average:.6f}")


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(prog="gwm")
    sub = parser.add_subparsers(dest="command", required=True)

    build = sub.add_parser("build", help="build an index from a gSpan database")
    build.add_argument(
        "-iteration", type=int, default=_DEFAULT_ITERATION,
        help=f"number of WLKernel iterations (default: {_DEFAULT_ITERATION})",
    )
    build.add_argument("database")
    build.add_argument("indexfile")
    build.set_defaults(func=_build)

    search = sub.add_parser("search", help="search an index with a gSpan query file")
    search.add_argument(
        "-kthreshold", type=float, default=_DEFAULT_THRESHOLD,
        help=f"cosine similarity threshold (default: {_DEFAULT_THRESHOLD})",
    )
    search.add_argument("indexfile")
    search.add_argument("queryfile")
    search.set_defaults(func=_search)

    ns = parser.parse_args(argv)
    ns.func(ns)
    return 0


if __name__ == "__main__":
    sys.exit(main())
