#!/bin/sh
# gfopenoverlay.library 1.0 (LIBS:) with the os32-gcc16 stove, and OverlayTest.
#   sh library/overlay/build-amiga.sh            (STOVE=, OUT=)
# Needs layers.library 47.111 or later at run time (GFOpenLayers with OL_COMPOSITE).
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
TOP=$(CDPATH= cd -- "$HERE/../.." && pwd)
STOVE=${STOVE:-"$HOME/AmiCradle/stoves/os32-gcc16"}
P="$STOVE/prefix"
CC=${CC:-"$P/bin/m68k-amigaos-gcc"}
OUT=${OUT:-"$HERE/build-amiga"}
mkdir -p "$OUT/libs" "$OUT/tests"
"$CC" -m68020 -Os -fno-strict-aliasing -fomit-frame-pointer -fno-toplevel-reorder -fno-lto -fno-builtin \
  -Wall -Wextra -Werror -Wno-unused-parameter -nostartfiles -nostdlib -I"$TOP/include" \
  -o "$OUT/libs/gfopenoverlay.library" "$HERE/gfopenoverlay_lib.c" -lgcc
echo "$OUT/libs/gfopenoverlay.library ($(wc -c < "$OUT/libs/gfopenoverlay.library") bytes)"
und=$("$P/bin/m68k-amigaos-nm" -u "$OUT/libs/gfopenoverlay.library" || true)
[ -z "$und" ] || { echo "undefined in gfopenoverlay.library: $und" >&2; exit 1; }
if [ -f "$TOP/tests/OverlayTest.c" ]; then
  "$CC" -m68020 -O2 -Wall -Wextra -Werror -Wno-pointer-sign -noixemul -I"$TOP/include" \
    -o "$OUT/tests/OverlayTest" "$TOP/tests/OverlayTest.c"
  echo "$OUT/tests/OverlayTest ($(wc -c < "$OUT/tests/OverlayTest") bytes)"
fi
