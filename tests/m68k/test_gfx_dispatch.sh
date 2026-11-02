#!/bin/sh
# GFOpenGfx graphics.library 47.101's drawing dispatch
# (library/graphics/gfx_dispatch.c) under qemu-m68k: built with an m68k
# cross GCC for the 68040 at -O0 and -O2 with the real face and core (the
# face's sixteen drawing calls) and stand-ins for text's four and gels' two,
# run by tests/m68k/test_gfx_dispatch.c, whose every line a python3 model
# written here, from include/gfopengpu/gfx.h's rules and the order of
# gfopenrtg's library/ogfx/ogfx_lib.c, checks: with nothing registered each
# call draws and answers as its owner's entry does; a provider of each
# record size (1.0, v1, 1.4 and longer) is asked for the calls its record
# has room for and no others, with the request as graphics.library reads
# the registers; a call it declines reaches the owner with the registers as
# they came; the look is asked first on RectFill and Text; a look that
# draws with RectFill gets the call back and declines it; the registry's
# rules (records refused, a second look refused, taking a look out while a
# call is inside it). Then the library checks: check_regs.py over the face
# and the dispatch, gen_stubs.py (22 slots dispatched, 178 in all), and nm:
# the dispatch has no .data, .bss or C++ runtime and calls only the owners'
# entries.
#   sh tests/m68k/test_gfx_dispatch.sh
set -eu
here=$(cd "$(dirname "$0")" && pwd)
top=$here/../..
g=$top/library/graphics
core=$top/library/core
cc=${M68K_CC:-m68k-linux-gnu-gcc}
cxx=${M68K_CXX:-m68k-linux-gnu-g++}
nm=${M68K_NM:-m68k-linux-gnu-nm}
qemu=${QEMU_M68K:-qemu-m68k}
rounds=${ROUNDS:-40}
command -v "$cc" >/dev/null || { echo "skipped: no $cc"; exit 0; }
command -v "$cxx" >/dev/null || { echo "skipped: no $cxx"; exit 0; }
command -v python3 >/dev/null || { echo "skipped: no python3"; exit 0; }
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
fail=0

cxxflags="-std=gnu++23 -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-asynchronous-unwind-tables
  -fno-unwind-tables -nostdlib++ -ffreestanding -fno-builtin -Wall -Wextra -Werror"
warn="-Wall -Wextra -Werror -Wno-unused-parameter"
inc="-I$g -I$core -I$here/dispatch-ndk -I$here/gfx_face_ndk -I$here/../host-ndk -I$top/include"

cat > "$tmp/model.py" <<'PY'
# The model: what each line of test_gfx_dispatch.c's output must say, from
# gfx.h's rules and ogfx_lib.c's order (look, then provider, then
# graphics.library's own code), worked out here and not taken from the
# library.
import sys

lines = open(sys.argv[1]).read().split("\n")
ROUNDS = int(sys.argv[2])
pos = 0
bad = 0
checks = 0
M = 0xFFFFFFFF

def err(msg):
    global bad
    bad += 1
    if bad <= 25: print("MODEL line %d: %s" % (pos, msg))

def nxt():
    global pos
    l = lines[pos]; pos += 1
    return l

def h(x): return int(x, 16)
def s16(v): v &= 0xFFFF; return (v - 0x10000 if v & 0x8000 else v) & M
def z16(v): return v & 0xFFFF
def z8(v): return v & 0xFF

ST_PATCHED, ST_ENABLED, ST_PROVIDER, ST_LOOK, ST_BUSY = 1, 2, 4, 16, 32

# the calls in ogfx_lib.c's OGFX_P_ order; their entries in OGFXProviderAll
NAMES = ["Text", "TextLength", "TextExtent", "TextFit", "RectFill", "BltBitMap", "BltTemplate", "ScrollRaster",
         "BltPattern", "SetRast", "Draw", "PolyDraw", "WritePixel", "ReadPixel", "BltBitMapRastPort",
         "BltMaskBitMapRastPort", "ClipBlit", "WriteChunkyPixels", "WritePixelArray8", "WritePixelLine8",
         "ReadPixelLine8", "ReadPixelArray8"]
SLOT = [1, 4, 5, 6, 0, 2, 7, 3] + list(range(8, 22))      # gfx.h: rectfill, text, bltbitmap, scrollraster, ...
FACE = {4, 5, 7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 18, 19, 20, 21}   # the rest are text's and gels'
RESULT = {0: "L", 1: "W", 3: "L", 5: "L", 12: "L", 13: "L", 18: "L", 19: "L", 20: "L", 21: "L"}

def request(c, a):
    """the request as ogfx_lib.c fills it from the registers"""
    if c in (0, 1): return [a[0], a[1], z16(a[2]), 0]
    if c == 2: return [a[0], a[1], z16(a[2]), a[3]]
    if c == 3: return [a[0], a[1], z16(a[2]), a[3], a[4], s16(a[5]), z16(a[6]), z16(a[7]), 0]
    if c == 4: return [a[0]] + [s16(v) for v in a[1:5]]
    if c == 5: return [a[0], s16(a[1]), s16(a[2]), a[3]] + [s16(v) for v in a[4:8]] + [z8(a[8]), z8(a[9]), a[10], 0]
    if c == 6: return [a[0], s16(a[1]), s16(a[2]), a[3]] + [s16(v) for v in a[4:8]]
    if c == 7: return [a[0]] + [s16(v) for v in a[1:7]]
    if c == 8: return [a[0], a[1]] + [s16(v) for v in a[2:6]] + [s16(a[6])]
    if c == 9: return [a[0], z8(a[1])]
    if c == 10: return [a[0], s16(a[1]), s16(a[2])]
    if c == 11: return [a[0], s16(a[1]), a[2]]
    if c in (12, 13): return [a[0], s16(a[1]), s16(a[2]), 0]
    if c in (14, 15): return [a[0], s16(a[1]), s16(a[2]), a[3]] + [s16(v) for v in a[4:8]] + [z8(a[8]), a[9] if c == 15 else 0]
    if c == 16: return [a[0], s16(a[1]), s16(a[2]), a[3]] + [s16(v) for v in a[4:8]] + [z8(a[8])]
    if c == 17: return [a[0]] + [z16(v) for v in a[1:5]] + [0, a[5], a[6], 0, 0]
    if c in (18, 21): return [a[0]] + [z16(v) for v in a[1:5]] + [0, a[5], 0, a[6], 0]
    if c in (19, 20):
        x0, y0, w = z16(a[1]), z16(a[2]), z16(a[3])
        return [a[0], x0, y0, (x0 + w - 1) & M, y0, w, a[4], 0, a[5], 0]

def area_answer(c, a):
    v = (sum(a) * 2654435761 + c) & M
    return s16(v) if c == 1 else v

# the registry's rules, line by line (test_gfx_dispatch.c rules())
RULES = """VER 00010005
ST 00000003
PR null 0
PR abi2 0
PR short 0
PR noowner 0
ST 00000003
PR a 1
PR b 1
UP c 0
UP null 0
UP a 0
ST 00000007
UP b 1
UP b 0
ST 00000003
LR null 0
LR abi2 0
LR short 0
LR noowner 0
LR a 1
LR b 0
ST 00000013
UL b 0
UL null 0
LOOK depth 0 busy 1 status 00000033
LOOK depth 1 busy 2 status 00000033
REENTER same 1 busy 0
ST 00000013
LOOK depth 0 busy 1 status 00000033
LOOKOUT 1 0 00000023
OUT same 1
ST 00000003
LR b 1
UL a 0
UL b 1
ST 00000003""".split("\n")

prov = None; look = None; take = 0; ltake = 0; scn = -1
for want in RULES:
    l = nxt(); checks += 1
    if l != want: err("rules: %r, want %r" % (l, want))

paths = {}
while pos < len(lines):
    l = nxt()
    if not l: continue
    f = l.split()
    t = f[0]
    checks += 1
    if t == "ERR": err(l)
    elif t == "END":
        if f[1] != "0": err("harness errors " + f[1])
    elif t == "FORBID":
        if f[1] != f[2] or f[3] != "0" or f[1] == "0": err("Forbid and Permit: " + l)
    elif t == "SCN":
        scn += 1
        pm, size, nullmask, lm, lnull, take, ltake = int(f[1]), int(f[2]), h(f[3]), int(f[4]), h(f[5]), h(f[6]), h(f[7])
        prov = (size, nullmask) if pm else None
        look = lnull if lm else None
        rnd = min(scn, ROUNDS) & 15
        if pm and nxt() != "REGP 1": err("provider of %d bytes refused" % size)
        if lm and nxt() != "REGL 1": err("look refused")
        st = h(nxt().split()[1])
        want = ST_PATCHED | ST_ENABLED | (ST_PROVIDER if pm else 0) | (ST_LOOK if lm else 0)
        if st != want: err("status %08x want %08x" % (st, want))
    elif t == "C":
        c, n = int(f[1]), int(f[2]); a = [h(x) for x in f[3:3 + n]]
        req = request(c, a)
        path = "area"
        # the look: RectFill and Text, first
        if c in (0, 4) and look is not None and not (look >> (0 if c == 4 else 1)) & 1:
            bit = 0 if c == 4 else 1
            g = nxt().split()
            taken = (ltake >> bit) & 1
            if g[:4] != ["ASK", "L", str(bit), str(taken)]: err("%s: look not asked: %s" % (NAMES[c], " ".join(g[:4])))
            elif [h(x) for x in g[4:]] != req: err("%s: look's request %s want %s" % (NAMES[c], g[4:], ["%08x" % x for x in req]))
            if taken: path = "look"
        # the provider: the calls its record has room for, a NULL entry not
        if path == "area" and prov is not None:
            size, nullmask = prov
            k = SLOT[c]
            if 16 + 4 * k + 4 <= min(size, 104) and not (nullmask >> k) & 1:
                g = nxt().split()
                taken = (take >> k) & 1
                if g[:4] != ["ASK", "P", str(k), str(taken)]: err("%s: provider not asked: %s" % (NAMES[c], " ".join(g[:4])))
                elif [h(x) for x in g[4:]] != req: err("%s: provider's request %s want %s" % (NAMES[c], g[4:], ["%08x" % x for x in req]))
                if taken: path = "provider"
        paths[(c, path)] = paths.get((c, path), 0) + 1
        if path == "area" and c not in FACE:
            g = nxt().split()
            if g[:2] != ["AREA", str(c)] or [h(x) for x in g[2:]] != a: err("%s: owner's entry got %s, want %s" % (NAMES[c], g, f[3:]))
        g = nxt().split()
        if g[0] != "R" or int(g[1]) != c: err("%s: %s, want its result" % (NAMES[c], " ".join(g))); continue
        r, rs, same, untouched = h(g[2]), h(g[3]), g[4], g[5]
        if path == "area":
            if c in FACE:
                if same != "1" or r != rs: err("%s: through the dispatch differs from the owner's entry: %s" % (NAMES[c], l))
            elif r != (area_answer(c, a) if c in RESULT else 0): err("%s: answered %08x, the owner %08x" % (NAMES[c], r, area_answer(c, a)))
        else:
            if c in FACE and untouched != "1": err("%s: taken by the %s, but drawn too" % (NAMES[c], path))
            if path == "look": want = (0x0bad0000 + min(scn, ROUNDS)) & M if c == 0 else 0
            elif c in RESULT:
                v = 0x12348000 + SLOT[c] * 16 + rnd
                want = s16(v) if RESULT[c] == "W" else v
            else: want = 0
            if r != want: err("%s: taken by the %s, answered %08x want %08x" % (NAMES[c], path, r, want))
    else:
        err("unknown line " + l[:60])

for c in range(22):
    for p in ("area", "provider") + (("look",) if c in (0, 4) else ()):
        if not paths.get((c, p)): err("%s never went to the %s" % (NAMES[c], p))
print("%d lines, %d model errors" % (checks, bad))
sys.exit(1 if bad else 0)
PY

for opt in -O0 -O2; do
  # shellcheck disable=SC2086
  "$cxx" -m68040 $opt $cxxflags -I"$g" -I"$core" -c -o "$tmp/face.o" "$g/gfx_face.cpp"
  # shellcheck disable=SC2086
  "$cc" -std=gnu11 -m68040 $opt $warn $inc -c -o "$tmp/dispatch.o" "$g/gfx_dispatch.c"
  "$cc" -std=gnu11 -m68040 $opt $warn -I"$g" -c -o "$tmp/lvo.o" "$g/face_lvo.c"
  # the dispatch: no data or bss, no C++ runtime, and it calls the owners' entries only
  data=$("$nm" "$tmp/dispatch.o" | awk '$2 ~ /^[dDbBcCgGsS]$/' || true)
  [ -z "$data" ] || { echo "FAIL $opt: data in the dispatch: $data"; fail=1; }
  und=$("$nm" -u "$tmp/dispatch.o" | awk '{print $2}' | grep -Ev '^gfx_lvo_[A-Za-z0-9]+$' || true)
  [ -z "$und" ] || { echo "FAIL $opt: the dispatch calls $und"; fail=1; }
  nund=$("$nm" -u "$tmp/dispatch.o" | grep -c 'gfx_lvo_' || true)
  [ "$nund" = 22 ] || { echo "FAIL $opt: the dispatch calls $nund owners' entries, not 22"; fail=1; }
  # shellcheck disable=SC2086
  "$cc" -m68040 $opt -static $warn -DROUNDS=$rounds $inc -o "$tmp/t" "$here/test_gfx_dispatch.c" \
    "$tmp/dispatch.o" "$tmp/lvo.o" "$core/gfx_core.c" "$core/gfx_region.c" "$core/gfx_rp.c" "$tmp/face.o"
  if timeout 300 "$qemu" -cpu m68040 "$tmp/t" > "$tmp/out"; then
    if res=$(python3 "$tmp/model.py" "$tmp/out" "$rounds"); then echo "ok   $opt -m68040: $res"
    else echo "FAIL $opt -m68040:"; echo "$res"; fail=1; fi
  else echo "FAIL $opt -m68040: the test stopped"; grep ERR "$tmp/out" | head; tail -3 "$tmp/out"; fail=1; fi
done

# the tables: the face's and the dispatch's register entries against
# graphics.lvo, and the vector table gen_stubs.py makes from them
python3 "$g/check_regs.py" "$g/graphics.lvo" "$g/OWNERS.md" "$g/face_lvo.c" "$g/gfx_dispatch.c" > "$tmp/regs" ||
  { cat "$tmp/regs"; fail=1; }
cut -c1-80 "$tmp/regs"
grep -q '22 dispatched, registers match' "$tmp/regs" || { echo "FAIL check_regs: not 22 dispatch entries"; fail=1; }
"$nm" "$tmp/lvo.o" "$tmp/dispatch.o" "$tmp/face.o" > "$tmp/defined"
python3 "$g/gen_stubs.py" "$g/graphics.lvo" "$tmp/defined" "$tmp" | cut -c1-80
n=$(grep -c '^    (APTR)gfx_dispatch_' "$tmp/gfx_vectors.h" || true)
[ "$n" = 22 ] || { echo "FAIL gfx_vectors.h dispatches $n slots, not 22"; fail=1; }
n=$(grep -c '^    (APTR)gfx_' "$tmp/gfx_vectors.h" || true)
[ "$n" = 178 ] || { echo "FAIL gfx_vectors.h has $n slots, not 178"; fail=1; }
for call in AreaMove AreaDraw AreaEllipse AreaEnd DrawEllipse EraseRect; do
  grep -q "^    (APTR)gfx_lvo_$call," "$tmp/gfx_vectors.h" || { echo "FAIL $call is not its owner's entry"; fail=1; }
done
python3 - "$tmp/gfx_vectors.h" <<'PY' || fail=1
import re, sys
v = [l for l in open(sys.argv[1]) if l.startswith("    (APTR)gfx_")]
last = [re.match(r"\s*\(APTR\)gfx_lvo_(\w+), /\* (-\d+)", l).groups() for l in v[-6:]]
want = [("GFX_DispatchVersion", "-1062"), ("GFX_RegisterProvider", "-1068"), ("GFX_UnregisterProvider", "-1074"),
        ("GFX_RegisterLook", "-1080"), ("GFX_UnregisterLook", "-1086"), ("GFX_DispatchStatus", "-1092")]
assert last == want, last
print("vectors: the six GF extension calls at -1062 to -1092")
PY
"$cc" -m68040 -O2 $warn -I"$g" -c -o "$tmp/stubs.o" "$tmp/gfx_stubs.c" || fail=1
[ $fail = 0 ] && echo "test_gfx_dispatch: all passed"
exit $fail
