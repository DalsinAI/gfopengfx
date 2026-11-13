#!/bin/sh
# GFOpenLayers with the os32-gcc16 stove: checks layers.lvo against the
# NDK 3.2 layers_lib.fd, then builds
#   layers.library       the replacement (47.100), a resident module
#   gfopenlayers.library   the same code under its own name, for the ABI test
#   LayersABITest        runs the same calls through both and lists every difference
# The amicradle-guest checkout with libraries/common/check_fd.py:
#   GUEST=~/src/amicradle-guest sh library/layers/build-amiga.sh
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
STOVE=${STOVE:-"$HOME/AmiCradle/stoves/os32-gcc16"}
P="$STOVE/prefix"
CC=${CC:-"$P/bin/m68k-amigaos-gcc"}
OUT=${OUT:-"$HERE/build-amiga"}
# the ROM module's optimisation: -Os (size) by default; OPT=-O2 for speed
OPT=${OPT:--Os}
GUEST=${GUEST:?set GUEST to an amicradle-guest checkout}
mkdir -p "$OUT/libs" "$OUT/tests"

# NDK 3.2's fd, never fd13
FD=${FD:-"$P/m68k-amigaos/ndk/lib/fd/layers_lib.fd"}
[ -f "$FD" ] || { echo "no $FD: set FD=" >&2; exit 1; }
python3 "$GUEST/libraries/common/check_fd.py" "$FD" "$HERE/layers.lvo"

LIBFLAGS="-m68040 $OPT -fno-strict-aliasing -fno-delete-null-pointer-checks -fomit-frame-pointer -fno-toplevel-reorder
  -fno-lto -fno-builtin -fno-tree-loop-distribute-patterns -Wall -Wextra -Werror
  -Wno-unused-parameter -nostartfiles -nostdlib"
# OL_COMPOSITE: the composited mode (layers_comp.c, docs/design/compositing.md),
# which lives in GFOpenLayout since 13 Nov 2026 (wave 2, review rule 4) and is
# taken from a checkout of it: LAYOUT=, else ../gfopenlayout beside this one,
# else a shallow clone of its master (LAYOUT_BRANCH=). COMPOSITE=0 builds classic layers only,
# as before it, and needs no GFOpenLayout.
COMP=${COMPOSITE:-1}
CINC=
if [ "$COMP" = 0 ]; then CDEF=; else
  CDEF=-DOL_COMPOSITE
  LAYOUT=${LAYOUT:-"$HERE/../../../gfopenlayout"}
  if [ ! -f "$LAYOUT/library/layers/layers_comp.c" ]; then
    LTMP=$(mktemp -d); trap 'rm -rf "$LTMP"' EXIT
    LAYOUT="$LTMP/gfopenlayout"
    git clone -q --depth 1 -b "${LAYOUT_BRANCH:-master}" https://github.com/DalsinAI/gfopenlayout "$LAYOUT"
  fi
  [ -f "$LAYOUT/library/layers/layers_comp.c" ] || { echo "no layers_comp.c in LAYOUT=$LAYOUT" >&2; exit 1; }
  echo "layers_comp.c from $LAYOUT ($(git -C "$LAYOUT" rev-parse --short HEAD 2>/dev/null || echo no git))"
  CINC="-I$LAYOUT/library/layers"
fi
for name in layers gfopenlayers; do
  # shellcheck disable=SC2086
  "$CC" $LIBFLAGS $CDEF -DLIBNAME="\"$name.library\"" -I"$HERE" $CINC -I"$HERE/../../include" -o "$OUT/libs/$name.library" \
    "$HERE/openlayers_lib.c" "$HERE/layers_lib.c" "$HERE/layers_core.c" -lgcc
  echo "$OUT/libs/$name.library ($(wc -c < "$OUT/libs/$name.library") bytes)"
  und=$("$P/bin/m68k-amigaos-nm" -u "$OUT/libs/$name.library" || true)
  [ -z "$und" ] || { echo "undefined in $name.library: $und" >&2; exit 1; }
done

if [ -f "$HERE/../../tests/LayersABITest.c" ]; then
  "$CC" -m68040 -specs="$HERE/../../build/m68k-soft-m68040.specs" -O2 -fno-delete-null-pointer-checks -Wall -Wextra -Werror -Wno-pointer-sign -Wno-format -noixemul \
    -o "$OUT/tests/LayersABITest" "$HERE/../../tests/LayersABITest.c"
  echo "$OUT/tests/LayersABITest ($(wc -c < "$OUT/tests/LayersABITest") bytes)"
fi
sha256sum "$OUT"/libs/* "$OUT"/tests/* 2>/dev/null || true
