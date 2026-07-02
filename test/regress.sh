#!/bin/sh
# Output-parity regression for gwm-build / gwm-search.
#
# Builds the binaries, runs them on dat/mutagen*, and diffs stdout against
# test/golden/ (lines containing "time" are excluded; everything else,
# including hit lists, similarities, "length of id list", "depth of wavelet
# matrix" and "total memory", must be byte-identical).
#
# Usage: test/regress.sh            run the regression
#        test/regress.sh --update   regenerate the golden files
set -e
cd "$(dirname "$0")/.."
ROOT=$(pwd)
SRC=$ROOT/src
DAT=$ROOT/dat
GOLD=$ROOT/test/golden
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

make -C "$SRC" >/dev/null
mkdir -p "$GOLD"

"$SRC/gwm-build" -iteration 2 "$DAT/mutagen.gsp" "$TMP/idx2" > "$TMP/build-it2.raw" 2>/dev/null
"$SRC/gwm-build" -iteration 3 "$DAT/mutagen.gsp" "$TMP/idx3" > "$TMP/build-it3.raw" 2>/dev/null
"$SRC/gwm-search" -kthreshold 0.8 "$TMP/idx2" "$DAT/mutagen_query.gsp" > "$TMP/search-q08.raw" 2>/dev/null
"$SRC/gwm-search" -kthreshold 0.6 "$TMP/idx2" "$DAT/mutagen.gsp"       > "$TMP/search-self06.raw" 2>/dev/null
"$SRC/gwm-search" -kthreshold 0.9 "$TMP/idx3" "$DAT/mutagen_query.gsp" > "$TMP/search-q09it3.raw" 2>/dev/null

status=0
for f in build-it2 build-it3 search-q08 search-self06 search-q09it3; do
    grep -v "time" "$TMP/$f.raw" > "$TMP/$f.txt"
    if [ "$1" = "--update" ]; then
        cp "$TMP/$f.txt" "$GOLD/$f.txt"
        echo "updated golden/$f.txt"
    elif diff -q "$GOLD/$f.txt" "$TMP/$f.txt" >/dev/null 2>&1; then
        echo "OK   $f"
    else
        echo "FAIL $f"
        diff "$GOLD/$f.txt" "$TMP/$f.txt" | head -5 || true
        status=1
    fi
done
exit $status
