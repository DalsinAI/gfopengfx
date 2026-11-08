#!/bin/sh
# GFOpenGfx graphics.library with the os32-gcc16 stove (GCC 16.2.0-open2):
# checks graphics.lvo against the NDK 3.2 graphics_lib.fd and every area's
# gfx_lvo_ registers against graphics.lvo, then builds
#   graphics.library    the replacement (47.100), a resident module
#   gfopengfx.library     the same code under its own name, for the ABI tests
#   GfxFaceABITest      the face's calls side by side with OS 3.2's
# from library/graphics (the face), library/core and each area's
# library/<area>/graphics.sources (OWNERS.md); gen_stubs.py stubs whatever
# no area defines yet.
#   GUEST=~/src/amicradle-guest sh library/graphics/build-amiga.sh
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
LIB=$(CDPATH= cd -- "$HERE/.." && pwd)
TOP=$(CDPATH= cd -- "$LIB/.." && pwd)
STOVE=${STOVE:-"$HOME/AmiCradle/stoves/os32-gcc16"}
P="$STOVE/prefix"
CC=${CC:-"$P/bin/m68k-amigaos-gcc"}
CXX=${CXX:-"$P/bin/m68k-amigaos-g++"}
NM=${NM:-"$P/bin/m68k-amigaos-nm"}
OBJDUMP=${OBJDUMP:-"$P/bin/m68k-amigaos-objdump"}
OUT=${OUT:-"$HERE/build-amiga"}
# the ROM module's optimisation: -Os (size) by default; OPT=-O2 for speed
OPT=${OPT:--Os}
GUEST=${GUEST:-}
mkdir -p "$OUT/libs" "$OUT/tests" "$OUT/obj"

# NDK 3.2's fd, never fd13
FD=${FD:-"$P/m68k-amigaos/ndk/lib/fd/graphics_lib.fd"}
[ -f "$FD" ] || { echo "no $FD: set FD=" >&2; exit 1; }
[ -z "$GUEST" ] || python3 "$GUEST/libraries/common/check_fd.py" "$FD" "$HERE/graphics.lvo"

COMMON="-m68040 $OPT -fno-strict-aliasing -fno-delete-null-pointer-checks -fomit-frame-pointer -fno-toplevel-reorder
  -fno-lto -fno-builtin -fno-tree-loop-distribute-patterns -Wall -Wextra -Werror -Wno-unused-parameter
  -nostartfiles -nostdlib -I$HERE -I$LIB/core"
CXXFLAGS="-std=gnu++23 -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-asynchronous-unwind-tables
  -fno-unwind-tables -nostdlib++ -ffreestanding"

# the sources: the face's and core's, then each area's list
SRCS="$HERE/face_lvo.c $HERE/gfx_dispatch.c $HERE/gfx_face.cpp $LIB/core/gfx_core.c $LIB/core/gfx_rp.c $LIB/core/gfx_region.c"
for area in display text gels; do
  list="$LIB/$area/graphics.sources"
  [ -f "$list" ] || { echo "$area: no graphics.sources yet, stubbed"; continue; }
  while read -r f; do
    case "$f" in ''|\#*) continue ;; esac
    SRCS="$SRCS $LIB/$area/$f"
  done < "$list"
done

C_SRCS=""
for f in $SRCS; do case "$f" in *.c) C_SRCS="$C_SRCS $f" ;; esac; done
# shellcheck disable=SC2086
python3 "$HERE/check_regs.py" "$HERE/graphics.lvo" "$HERE/OWNERS.md" $C_SRCS

OBJS=""
for f in $SRCS; do
  o="$OUT/obj/$(basename "$(dirname "$f")")_$(basename "$f").o"
  # the dispatch takes its records from the SDK header (include/graphics/gfxdispatch.h)
  inc=""; [ "$f" = "$HERE/gfx_dispatch.c" ] && inc="-I$TOP/include"
  case "$f" in
    *.cpp) "$CXX" $COMMON $CXXFLAGS -c -o "$o" "$f" ;;
    *) "$CC" $COMMON $inc -c -o "$o" "$f" ;;
  esac
  OBJS="$OBJS $o"
done
# shellcheck disable=SC2086
"$NM" $OBJS | awk '$2 ~ /^[TD]$/ {print $3}' > "$OUT/obj/defined.txt"
python3 "$HERE/gen_stubs.py" "$HERE/graphics.lvo" "$OUT/obj/defined.txt" "$OUT/obj"
"$CC" $COMMON -c -o "$OUT/obj/stubs.o" "$OUT/obj/gfx_stubs.c"

for name in graphics gfopengfx; do
  # shellcheck disable=SC2086
  "$CC" $COMMON -I"$OUT/obj" -DLIBNAME="\"$name.library\"" -o "$OUT/libs/$name.library" \
    "$HERE/opengfx_lib.c" $OBJS "$OUT/obj/stubs.o" -lgcc
  echo "$OUT/libs/$name.library ($(wc -c < "$OUT/libs/$name.library") bytes)"
  und=$("$NM" -u "$OUT/libs/$name.library" || true)
  [ -z "$und" ] || { echo "undefined in $name.library: $und" >&2; exit 1; }
  data=$("$NM" "$OUT/libs/$name.library" | awk '$2 ~ /^[dDbBcC]$/' || true)
  [ -z "$data" ] || { echo "data or bss in $name.library: $data" >&2; exit 1; }
  bad=$("$NM" "$OUT/libs/$name.library" | grep -E '__cxa_|__gxx_personality|_ZTI|_ZTS|_GLOBAL__sub_I_|_ZGV|_Znw|_Zdl' || true)
  [ -z "$bad" ] || { echo "C++ runtime in $name.library: $bad" >&2; exit 1; }
  # the dispatch (gfx_dispatch.c): 22 drawing slots point at it, 179 slots in all
  n=$(grep -c '(APTR)gfx_dispatch_' "$OUT/obj/gfx_vectors.h")
  [ "$n" = 22 ] || { echo "$name.library: $n dispatched slots, not 22" >&2; exit 1; }
  # the A5 LVOs (face_lvo.c): each stub pushes A6, then A5 itself
  for call in LockLayerRom UnlockLayerRom AttemptLockLayerRom; do
    "$OBJDUMP" -d "$OUT/libs/$name.library" |
      awk -v l="<_gfx_lvo_$call>:" '$2 == l { n = 2; next } n { s = s " " $2; n-- } END { exit s != " 2f0e 2f0d" }' ||
      { echo "$name.library: gfx_lvo_$call does not start by pushing A6 and A5" >&2; exit 1; }
  done
done

"$CC" -m68040 -specs="$TOP/build/m68k-soft-m68040.specs" -O2 -fno-delete-null-pointer-checks -Wall -Wextra -Werror -Wno-pointer-sign -Wno-format -noixemul \
  -o "$OUT/tests/GfxFaceABITest" "$TOP/tests/GfxFaceABITest.c"

# the SDK (include/graphics/gfxdispatch.h, proto/inline): a provider and a
# look built against it, alone and with gfopengpu's gfx.h first (RTG= a
# gfopenrtg checkout, or this repository's copy); each call is its LVO
for first in none "$TOP/include" ${RTG:+"$RTG/include"}; do
  pre=""; [ "$first" = none ] || pre="-include $first/gfopengpu/gfx.h"
  # shellcheck disable=SC2086
  "$CC" -m68040 -O2 -Wall -Wextra -Werror -noixemul $pre -I"$TOP/include" -c -o "$OUT/obj/sdk_check.o" "$TOP/tests/GfxDispatchSDK.c"
done
for lvo in 1062 1068 1074 1080 1086 1092 1098; do
  "$OBJDUMP" -d "$OUT/obj/sdk_check.o" | grep -Eq "jsr %?a6@\(-$lvo\)" ||
    { echo "GfxDispatchSDK: no call to -$lvo(a6)" >&2; exit 1; }
done
echo "SDK: gfxdispatch.h, clib, inline and proto build, alone and after gfx.h; seven LVOs called"
echo "$OUT/tests/GfxFaceABITest ($(wc -c < "$OUT/tests/GfxFaceABITest") bytes)"
sha256sum "$OUT"/libs/* "$OUT"/tests/* 2>/dev/null || true
