#!/bin/sh
# OpenGfx's regions (library/core/gfx_region.c) against the regions as they
# were at 6c8b6d90 (gfx_region_ref.c, every call a sweep of the whole
# region), built with an m68k cross GCC for the 68040 at -O0 and -O2 and run
# under qemu-m68k. Thousands of random calls are made on both, the same
# ones: Rect calls (rectangles small, large, at the WORD's ends, empty,
# below the last band, inside the region) and Region calls, on three
# regions, and a list made by hand. After each call the answers must be
# the same, and so must the regions, rectangle by rectangle, bounds and
# links. Then 400 scattered rectangles ORed into one region, counting the
# allocations of each.
#   sh tests/m68k/test_gfx_region_ref.sh
set -eu
here=$(cd "$(dirname "$0")" && pwd)
core=$here/../../library/core
cc=${M68K_CC:-m68k-linux-gnu-gcc}
qemu=${QEMU_M68K:-qemu-m68k}
command -v "$cc" >/dev/null || { echo "skipped: no $cc"; exit 0; }
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
fail=0

cat > "$tmp/t.c" <<'C'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gfx_region.h"
void ref_rgn_init(struct gfx_region *r);
void ref_rgn_clear(struct gfx_region *r, const struct gfx_rgn_mem *m);
int ref_rgn_rect(struct gfx_region *dst, int op, const struct gfx_rect *rect, const struct gfx_rgn_mem *m);
int ref_rgn_region(struct gfx_region *dst, int op, const struct gfx_region *src, const struct gfx_rgn_mem *m);

static uint32_t seed = 0x5A17;
static uint32_t rnd(void) { seed = seed * 1103515245u + 12345u; return seed >> 8; }
static unsigned long calls, fails;
static long live, allocs;
static void *mem_alloc(void *ctx, uint32_t size)
{
    uint32_t *p = (uint32_t *)malloc(size + 8);
    (void)ctx;
    if (!p) return 0;
    p[0] = size;
    live += size;
    allocs++;
    return p + 2;
}
static void mem_free(void *ctx, void *q, uint32_t size)
{
    uint32_t *p = (uint32_t *)q - 2;
    (void)ctx;
    if (p[0] != size) { printf("FAIL free size %lu for %lu\n", (unsigned long)size, (unsigned long)p[0]); fails++; }
    live -= size;
    free(p);
}
static const struct gfx_rgn_mem mem = { mem_alloc, mem_free, 0 };

static struct gfx_region ours[3], ref[3];

static void show(const char *who, const struct gfx_region *r)
{
    const struct gfx_rrect *p;
    int n = 0;
    printf("  %s: bounds %d,%d-%d,%d:", who, r->bounds.min_x, r->bounds.min_y, r->bounds.max_x, r->bounds.max_y);
    for (p = r->rects; p && n < 16; p = p->next, n++)
        printf(" %d,%d-%d,%d", p->bounds.min_x, p->bounds.min_y, p->bounds.max_x, p->bounds.max_y);
    printf(p ? " ...\n" : "\n");
}

/* the same answer, the same rectangles in the same order, linked both ways */
static void same(int i, int ra, int rb, const char *what)
{
    const struct gfx_rrect *p = ours[i].rects, *q = ref[i].rects, *pp = 0;
    int bad = ra != rb || memcmp(&ours[i].bounds, &ref[i].bounds, sizeof ours[i].bounds);
    calls++;
    for (; !bad && p && q; pp = p, p = p->next, q = q->next)
        if (memcmp(&p->bounds, &q->bounds, sizeof p->bounds) || p->prev != pp) bad = 1;
    if (p || q) bad = 1;
    if (bad && ++fails <= 10) {
        printf("FAIL call %lu, %s on region %d: answers %d and %d\n", calls, what, i, ra, rb);
        show("ours", &ours[i]);
        show("then", &ref[i]);
    }
}

static int16_t clamp(int32_t v) { return (int16_t)(v < -32768 ? -32768 : v > 32767 ? 32767 : v); }

static void rand_rect(struct gfx_rect *rc, const struct gfx_region *r)
{
    int kind = (int)(rnd() % 16);
    int32_t x0, y0, w, h;
    if (kind < 7) {             /* small, in a small place: many meet */
        x0 = -24 + (int32_t)(rnd() % 96); y0 = -16 + (int32_t)(rnd() % 64);
        w = (int32_t)(rnd() % 40); h = (int32_t)(rnd() % 28);
    } else if (kind < 9) {      /* larger */
        x0 = -300 + (int32_t)(rnd() % 600); y0 = -200 + (int32_t)(rnd() % 400);
        w = (int32_t)(rnd() % 400); h = (int32_t)(rnd() % 300);
    } else if (kind == 9) {     /* at the WORD's ends */
        x0 = rnd() & 1 ? -32768 + (int32_t)(rnd() % 40) : 32727 + (int32_t)(rnd() % 40);
        y0 = rnd() & 1 ? -32768 + (int32_t)(rnd() % 40) : -16 + (int32_t)(rnd() % 64);
        w = rnd() % 4 ? (int32_t)(rnd() % 50) : 65535; h = (int32_t)(rnd() % 30);
    } else if (kind < 13 && r->rects) {     /* below the last band, often touching it */
        x0 = r->bounds.min_x - 10 + (int32_t)(rnd() % 60); y0 = r->bounds.max_y + 1 + (int32_t)(rnd() % 3);
        w = (int32_t)(rnd() % 40); h = (int32_t)(rnd() % 12);
        if (rnd() % 3 == 0) {   /* the last rectangle's run again */
            const struct gfx_rrect *p = r->rects;
            while (p->next) p = p->next;
            x0 = r->bounds.min_x + (uint16_t)p->bounds.min_x;
            w = (uint16_t)p->bounds.max_x - (uint16_t)p->bounds.min_x;
        }
    } else if (kind < 15 && r->rects) {     /* inside one of its rectangles */
        const struct gfx_rrect *p = r->rects;
        int32_t n = (int32_t)(rnd() % 8), ax0, ay0, ax1, ay1;
        while (n-- && p->next) p = p->next;
        ax0 = r->bounds.min_x + (uint16_t)p->bounds.min_x; ax1 = r->bounds.min_x + (uint16_t)p->bounds.max_x;
        ay0 = r->bounds.min_y + (uint16_t)p->bounds.min_y; ay1 = r->bounds.min_y + (uint16_t)p->bounds.max_y;
        x0 = ax0 + (int32_t)(rnd() % (uint32_t)(ax1 - ax0 + 1));
        y0 = ay0 + (int32_t)(rnd() % (uint32_t)(ay1 - ay0 + 1));
        w = (int32_t)(rnd() % (uint32_t)(ax1 - x0 + 1)); h = (int32_t)(rnd() % (uint32_t)(ay1 - y0 + 1));
    } else {                    /* empty */
        x0 = -24 + (int32_t)(rnd() % 96); y0 = -16 + (int32_t)(rnd() % 64);
        w = -1 - (int32_t)(rnd() % 5); h = (int32_t)(rnd() % 10);
        if (rnd() & 1) { int32_t t = w; w = h; h = t; }
    }
    rc->min_x = clamp(x0);
    rc->min_y = clamp(y0);
    rc->max_x = clamp(x0 + w);
    rc->max_y = clamp(y0 + h);
}

static const char *const opname[] = { "OrRect", "AndRect", "XorRect", "ClearRect" };
static const char *const rgnname[] = { "OrRegion", "AndRegion", "XorRegion" };

int main(int argc, char **argv)
{
    int i, k, op, rounds = argc > 1 ? atoi(argv[1]) : 20000;
    for (i = 0; i < 3; i++) { gfx_rgn_init(&ours[i]); ref_rgn_init(&ref[i]); }
    for (k = 0; k < rounds; k++) {
        int d = (int)(rnd() % 3), s = (int)(rnd() % 3), kind = (int)(rnd() % 100);
        if (kind < 3) {
            gfx_rgn_clear(&ours[d], &mem);
            ref_rgn_clear(&ref[d], &mem);
            same(d, 1, 1, "ClearRegion");
        } else if (kind < 90) {
            struct gfx_rect rc;
            op = kind < 45 ? GFX_RGN_OR : kind < 55 ? GFX_RGN_AND : kind < 70 ? GFX_RGN_XOR : GFX_RGN_CLEAR;
            rand_rect(&rc, &ours[d]);
            same(d, gfx_rgn_rect(&ours[d], op, &rc, &mem), ref_rgn_rect(&ref[d], op, &rc, &mem), opname[op]);
        } else {
            op = (int)(rnd() % 3);
            if (op == GFX_RGN_XOR && s == d) s = (d + 1) % 3;
            same(d, gfx_rgn_region(&ours[d], op, &ours[s], &mem), ref_rgn_region(&ref[d], op, &ref[s], &mem),
                 rgnname[op]);
        }
    }
    for (i = 0; i < 3; i++) { gfx_rgn_clear(&ours[i], &mem); ref_rgn_clear(&ref[i], &mem); }

    /* a list made by hand, as OS 3.2 keeps one: overlapping, newest first,
     * loose bounds; both make it bands */
    for (k = 0; k < 300; k++) {
        struct gfx_rect rc;
        int n = 1 + (int)(rnd() % 6), w;
        uint32_t s0 = seed;
        for (w = 0; w < 2; w++) {
            struct gfx_region *r = w ? &ref[0] : &ours[0];
            seed = s0;
            r->bounds.min_x = -5; r->bounds.min_y = -3; r->bounds.max_x = 90; r->bounds.max_y = 70;
            r->rects = 0;
            for (i = 0; i < n; i++) {
                struct gfx_rrect *p = (struct gfx_rrect *)mem_alloc(0, sizeof *p);
                /* empty ones too, but in WORD coordinates: an offset is read 0 to 65535 */
                p->bounds.min_x = (int16_t)(2 + rnd() % 50); p->bounds.min_y = (int16_t)(rnd() % 40);
                p->bounds.max_x = (int16_t)(p->bounds.min_x + (int)(rnd() % 30) - 2);
                p->bounds.max_y = (int16_t)(p->bounds.min_y + (int)(rnd() % 20));
                p->prev = 0; p->next = r->rects;
                if (r->rects) r->rects->prev = p;
                r->rects = p;
            }
        }
        rc.min_x = (int16_t)(-10 + (int)(rnd() % 100)); rc.min_y = (int16_t)(-10 + (int)(rnd() % 80));
        rc.max_x = (int16_t)(rc.min_x + (int)(rnd() % 40) - 3); rc.max_y = (int16_t)(rc.min_y + (int)(rnd() % 30));
        op = (int)(rnd() % 4);
        same(0, gfx_rgn_rect(&ours[0], op, &rc, &mem), ref_rgn_rect(&ref[0], op, &rc, &mem), "a list made by hand");
        gfx_rgn_clear(&ours[0], &mem);
        ref_rgn_clear(&ref[0], &mem);
    }
    if (live) { printf("FAIL %ld bytes kept\n", live); fails++; }

    /* 400 scattered rectangles ORed into one region */
    {
        struct gfx_rect rc[400];
        long a0, a1;
        for (i = 0; i < 400; i++) {
            rc[i].min_x = (int16_t)(rnd() % 640); rc[i].min_y = (int16_t)(rnd() % 480);
            rc[i].max_x = (int16_t)(rc[i].min_x + 1 + (int)(rnd() % 48));
            rc[i].max_y = (int16_t)(rc[i].min_y + 1 + (int)(rnd() % 32));
        }
        gfx_rgn_init(&ours[0]); ref_rgn_init(&ref[0]);
        allocs = 0;
        for (i = 0; i < 400; i++) gfx_rgn_rect(&ours[0], GFX_RGN_OR, &rc[i], &mem);
        a0 = allocs;
        allocs = 0;
        for (i = 0; i < 400; i++) ref_rgn_rect(&ref[0], GFX_RGN_OR, &rc[i], &mem);
        a1 = allocs;
        same(0, 1, 1, "400 scattered rectangles");
        printf("400 scattered rectangles: %ld allocations, %ld before; ", a0, a1);
        if (a0 * 20 > a1) { printf("FAIL not a twentieth of them\n"); fails++; }
        gfx_rgn_clear(&ours[0], &mem); ref_rgn_clear(&ref[0], &mem);
    }
    printf("%lu calls compared, %lu failed\n", calls, fails);
    return fails != 0;
}
C

for cpu in -m68040; do
  for opt in -O0 -O2; do
    "$cc" $cpu $opt -Wall -Wextra -Werror -I"$core" -Dgfx_rgn_init=ref_rgn_init -Dgfx_rgn_clear=ref_rgn_clear \
      -Dgfx_rgn_rect=ref_rgn_rect -Dgfx_rgn_region=ref_rgn_region -c -o "$tmp/ref.o" "$here/gfx_region_ref.c"
    "$cc" $cpu $opt -static -Wall -Wextra -Werror -I"$core" -o "$tmp/t" "$tmp/t.c" "$core/gfx_region.c" "$tmp/ref.o"
    q=m${cpu#-m}
    if out=$("$qemu" -cpu $q "$tmp/t"); then echo "ok   $opt $cpu: $out"; else echo "FAIL $opt $cpu:"; echo "$out"; fail=1; fi
  done
done
exit $fail
