#!/bin/sh
# GFOS face log (gfosmodules/gfos_facelog.c): the unit tests, built for the host and, with an m68k cross
# GCC, for the 68040 at -O0 and -O2 under qemu-m68k.
#   sh tests/m68k/test_facelog.sh
set -eu
here=$(cd "$(dirname "$0")" && pwd)
mods=$here/../../gfosmodules
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
fail=0
hcc=${HOST_CC:-cc}
if command -v "$hcc" >/dev/null; then
  "$hcc" -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -I"$mods" -o "$tmp/h" "$here/test_facelog.c"
  if out=$("$tmp/h"); then echo "ok   host: $out"; else echo "FAIL host:"; echo "$out"; fail=1; fi
fi
cc=${M68K_CC:-m68k-linux-gnu-gcc}
qemu=${QEMU_M68K:-qemu-m68k}
if command -v "$cc" >/dev/null; then
  for opt in -O0 -O2; do
    "$cc" -m68040 $opt -static -Wall -Wextra -Werror -I"$mods" -o "$tmp/t" "$here/test_facelog.c"
    if out=$("$qemu" -cpu m68040 "$tmp/t"); then echo "ok   $opt -m68040: $out"; else echo "FAIL $opt -m68040:"; echo "$out"; fail=1; fi
  done
else
  echo "skipped m68k: no $cc"
fi
exit $fail
