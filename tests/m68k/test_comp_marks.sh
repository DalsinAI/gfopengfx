#!/bin/sh
# GFOpenLayers' composited mode: what each drawing call marks
# (library/layers/layers_mark.h) against what GFOpenGfx's core and text draw,
# under qemu-m68k on the 68040 at -O0 and -O2 (tests/m68k/test_comp_marks.c).
#   sh tests/m68k/test_comp_marks.sh
set -eu
here=$(cd "$(dirname "$0")" && pwd)
top=$here/../..
core=$top/library/core
text=$top/library/text
cc=${M68K_CC:-m68k-linux-gnu-gcc}
cxx=${M68K_CXX:-m68k-linux-gnu-g++}
qemu=${QEMU_M68K:-qemu-m68k}
command -v "$cc" >/dev/null || { echo "skipped: no $cc"; exit 0; }
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
fail=0
warn="-Wall -Wextra -Werror"
inc="-I$here/text-ndk -I$here/../host-ndk -I$core -I$text -I$top/library/layers -include $here/text-ndk/gfxdefs.h"
cxxflags="-std=gnu++23 -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-asynchronous-unwind-tables
  -fno-unwind-tables -nostdlib++ -ffreestanding -fno-builtin $warn"
for cpu in -m68040; do
  for opt in -O0 -O2; do
    # shellcheck disable=SC2086
    "$cxx" $cpu $opt $cxxflags -c "$text/text_core.cpp" -o "$tmp/text_core.o"
    # shellcheck disable=SC2086
    "$cxx" $cpu $opt $cxxflags -c "$text/topaz_data.cpp" -o "$tmp/topaz_data.o"
    # shellcheck disable=SC2086
    "$cc" $cpu $opt -static $warn -Wno-unused-parameter $inc -o "$tmp/t" "$here/test_comp_marks.c" \
      "$core/gfx_rp.c" "$core/gfx_core.c" "$tmp/text_core.o" "$tmp/topaz_data.o"
    if out=$(timeout 300 "$qemu" -cpu m${cpu#-m} "$tmp/t"); then echo "ok   $opt $cpu: $out"; else echo "FAIL $opt $cpu:"; echo "$out"; fail=1; fi
  done
done
exit $fail
