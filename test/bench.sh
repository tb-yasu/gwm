#!/bin/sh
# Benchmark on a 50x-replicated mutagen database (34,000 graphs).
# Set BENCH_DIR to reuse the replicated file across runs.
set -e
cd "$(dirname "$0")/.."
ROOT=$(pwd)
SRC=$ROOT/src
DAT=$ROOT/dat
TMP=${BENCH_DIR:-$(mktemp -d)}
BIG=$TMP/big.gsp

if [ ! -f "$BIG" ]; then
    i=0
    while [ $i -lt 50 ]; do
        cat "$DAT/mutagen.gsp"
        printf '\n\n'
        i=$((i + 1))
    done > "$BIG"
fi

make -C "$SRC" >/dev/null

echo "== gwm-build -iteration 2 (34k graphs) =="
/usr/bin/time "$SRC/gwm-build" -iteration 2 "$BIG" "$TMP/idx" \
    >"$TMP/build.out" 2>"$TMP/build.err"
grep -E "cpu time|total" "$TMP/build.out"
grep " real " "$TMP/build.err"

for th in 0.8 0.6; do
    echo "== gwm-search -kthreshold $th (680 queries) =="
    "$SRC/gwm-search" -kthreshold "$th" "$TMP/idx" "$DAT/mutagen.gsp" \
        2>/dev/null | tail -4
done
