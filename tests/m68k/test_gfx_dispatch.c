/* SPDX-License-Identifier: MIT */
/* GFOpenGfx graphics.library 47.102: the drawing dispatch
 * (library/graphics/gfx_dispatch.c) over the real face and core for the
 * face's sixteen drawing calls, and over recording stand-ins for text's
 * four and gels' two. Each call runs twice from the same start: straight
 * into its owner's gfx_lvo_<Call> and through gfx_dispatch_<Call>, as the
 * vector table has it. Every line printed is checked by the python model
 * in test_gfx_dispatch.sh, written apart from the library from gfx.h's
 * rules, ogfx_lib.c's order and gfxdispatch.h's provider chain: who is
 * asked, in what order, with what request, who draws, and what the call
 * answers. Copyright (c) 2026 AmiCradle Team. MIT. */
#include <stdio.h>
#include <string.h>
#include "gfx_face.h"
#include "gfx_core.h"
#include <graphics/gfxdispatch.h>

#ifndef ROUNDS
#define ROUNDS 40
#endif

typedef uint32_t U;

static uint32_t seed = 0xd15a7c;
static uint32_t rnd(void) { seed = seed * 1103515245u + 12345u; return (seed >> 8) ^ (seed << 13); }
static uint32_t rn(uint32_t n) { return n ? rnd() % n : 0; }
static int errors;
static void fail(const char *what, long a) { printf("ERR %s %ld\n", what, a); errors++; }

/* ---- the library base, the env ------------------------------------------ */

static uint8_t basemem[GFX_BASE_POSSIZE] __attribute__((aligned(4)));
#define GB ((void *)basemem)
static int forbids, permits, forbid_depth;
static void t_forbid(void *c) { (void)c; forbids++; forbid_depth++; }
static void t_permit(void *c) { (void)c; permits++; if (--forbid_depth < 0) fail("permit unbalanced", 0); }
static void t_nop(void *c) { (void)c; }
static uint8_t arena[1 << 16] __attribute__((aligned(8)));
static uint32_t used;
static void *t_alloc(void *c, uint32_t size, uint32_t f)
{
    void *p;
    (void)c;
    if (used + size + 8 > sizeof arena) return 0;
    p = arena + used;
    if (f & GFX_MEMF_CLEAR) memset(p, 0, size);
    used = (used + size + 7) & ~7u;
    return p;
}
static void t_free(void *c, void *p, uint32_t s) { (void)c; (void)p; (void)s; }
static void t_sem(void *c, struct gfx_semaphore *s) { (void)c; (void)s; }
static int32_t t_sem_attempt(void *c, struct gfx_semaphore *s) { (void)c; (void)s; return 1; }

void gfx_hook_vp_colors(struct gfx_env *e, void *vp, uint32_t first, uint32_t count)
{ (void)e; (void)vp; (void)first; (void)count; }
void gfx_hook_set_font(struct gfx_env *e, struct gfx_rastport *rp, void *font) { (void)e; rp->font = font; }
int32_t gfx_hook_blt_bitmap(struct gfx_env *e, struct gfx_bitmap *s, int32_t sx, int32_t sy, struct gfx_bitmap *d,
                            int32_t dx, int32_t dy, int32_t w, int32_t h, uint32_t mt, uint32_t m, uint8_t *t)
{
    (void)e; (void)s; (void)sx; (void)sy; (void)d; (void)dx; (void)dy; (void)w; (void)h; (void)mt; (void)m; (void)t;
    return -1;
}
void gfx_hook_wait_blit(struct gfx_env *e) { (void)e; }

/* ---- the entries ------------------------------------------------------------
 * Every argument is a 32-bit register (a stack word here); A6 last. */
U gfx_dispatch_Text(U, U, U, void *);              U gfx_lvo_Text(U, U, U, void *);
int16_t gfx_dispatch_TextLength(U, U, U, void *);
void gfx_dispatch_TextExtent(U, U, U, U, void *);
U gfx_dispatch_TextFit(U, U, U, U, U, U, U, U, void *);
void gfx_dispatch_RectFill(U, U, U, U, U, void *);   void gfx_lvo_RectFill(U, U, U, U, U, void *);
U gfx_dispatch_BltBitMap(U, U, U, U, U, U, U, U, U, U, U, void *);
U gfx_lvo_BltBitMap(U, U, U, U, U, U, U, U, U, U, U, void *);
void gfx_dispatch_BltTemplate(U, U, U, U, U, U, U, U, void *);
void gfx_dispatch_ScrollRaster(U, U, U, U, U, U, U, void *); void gfx_lvo_ScrollRaster(U, U, U, U, U, U, U, void *);
void gfx_dispatch_BltPattern(U, U, U, U, U, U, U, void *);   void gfx_lvo_BltPattern(U, U, U, U, U, U, U, void *);
void gfx_dispatch_SetRast(U, U, void *);              void gfx_lvo_SetRast(U, U, void *);
void gfx_dispatch_Draw(U, U, U, void *);              void gfx_lvo_Draw(U, U, U, void *);
void gfx_dispatch_PolyDraw(U, U, U, void *);          void gfx_lvo_PolyDraw(U, U, U, void *);
U gfx_dispatch_WritePixel(U, U, U, void *);           U gfx_lvo_WritePixel(U, U, U, void *);
U gfx_dispatch_ReadPixel(U, U, U, void *);            U gfx_lvo_ReadPixel(U, U, U, void *);
void gfx_dispatch_BltBitMapRastPort(U, U, U, U, U, U, U, U, U, void *);
void gfx_lvo_BltBitMapRastPort(U, U, U, U, U, U, U, U, U, void *);
void gfx_dispatch_BltMaskBitMapRastPort(U, U, U, U, U, U, U, U, U, U, void *);
void gfx_dispatch_ClipBlit(U, U, U, U, U, U, U, U, U, void *); void gfx_lvo_ClipBlit(U, U, U, U, U, U, U, U, U, void *);
void gfx_dispatch_WriteChunkyPixels(U, U, U, U, U, U, U, void *); void gfx_lvo_WriteChunkyPixels(U, U, U, U, U, U, U, void *);
U gfx_dispatch_WritePixelArray8(U, U, U, U, U, U, U, void *); U gfx_lvo_WritePixelArray8(U, U, U, U, U, U, U, void *);
U gfx_dispatch_WritePixelLine8(U, U, U, U, U, U, void *);    U gfx_lvo_WritePixelLine8(U, U, U, U, U, U, void *);
U gfx_dispatch_ReadPixelLine8(U, U, U, U, U, U, void *);     U gfx_lvo_ReadPixelLine8(U, U, U, U, U, U, void *);
U gfx_dispatch_ReadPixelArray8(U, U, U, U, U, U, U, void *); U gfx_lvo_ReadPixelArray8(U, U, U, U, U, U, U, void *);
U gfx_lvo_GFX_DispatchVersion(void *);
U gfx_lvo_GFX_RegisterProvider(void *, void *);
U gfx_lvo_GFX_UnregisterProvider(void *, void *);
U gfx_lvo_GFX_RegisterLook(void *, void *);
U gfx_lvo_GFX_UnregisterLook(void *, void *);
U gfx_lvo_GFX_DispatchStatus(void *);
U gfx_lvo_GFX_RegisterProviderPri(void *, U, void *);

/* quiet: the chain rules' calls (rules()), which record who was asked in
 * order[] instead of printing each ask */
static int quiet;
static char order[32];
static int norder;

/* text's and gels' entries: they say what they got and answer from it */
static int in_area = -1;               /* which call the stand-in expects; -1 none */
static U area_answer(int call, int n, const U *a)
{
    U s = 0;
    int i;
    if (!quiet) printf("AREA %d", call);
    for (i = 0; i < n; i++) { if (!quiet) printf(" %08lx", (unsigned long)a[i]); s += a[i]; }
    if (!quiet) printf("\n");
    if (call != in_area) fail("area stand-in reached for another call", call);
    return s * 2654435761u + (U)call;
}
U gfx_lvo_Text(U rp, U t, U n, void *gb)
{ U a[3] = { rp, t, n }; (void)gb; return area_answer(0, 3, a); }
int16_t gfx_lvo_TextLength(U rp, U t, U n, void *gb)
{ U a[3] = { rp, t, n }; (void)gb; return (int16_t)area_answer(1, 3, a); }
void gfx_lvo_TextExtent(U rp, U t, U n, U e, void *gb)
{ U a[4] = { rp, t, n, e }; (void)gb; (void)area_answer(2, 4, a); }
U gfx_lvo_TextFit(U rp, U t, U n, U e, U c, U d, U w, U h, void *gb)
{ U a[8] = { rp, t, n, e, c, d, w, h }; (void)gb; return area_answer(3, 8, a); }
void gfx_lvo_BltTemplate(U s, U sx, U m, U rp, U dx, U dy, U w, U h, void *gb)
{ U a[8] = { s, sx, m, rp, dx, dy, w, h }; (void)gb; (void)area_answer(6, 8, a); }
void gfx_lvo_BltMaskBitMapRastPort(U s, U sx, U sy, U rp, U dx, U dy, U w, U h, U mt, U mask, void *gb)
{ U a[10] = { s, sx, sy, rp, dx, dy, w, h, mt, mask }; (void)gb; (void)area_answer(15, 10, a); }

/* ---- two worlds: the straight call draws in A, the dispatched one in B ---- */

#define W 64
#define H 32
#define BPR 8
struct world {
    uint8_t planes[2][3][BPR * H];
    uint8_t tplanes[3][BPR * 2];
    uint8_t array[W * H + 64];
    uint8_t mask[BPR * H];
    int16_t poly[16];
    uint8_t text[64];
    int16_t te[2][6];
    struct gfx_bitmap bm[2], tbm;
    struct gfx_rastport rp, srp, trp;
} __attribute__((aligned(4)));
static struct world wa, wb, pre;

static void world_fill(struct world *w, uint32_t s)
{
    uint32_t x = s;
    uint8_t *p;
    int i, k;
    memset(w, 0, sizeof *w);
    for (p = &w->planes[0][0][0]; p < (uint8_t *)&w->poly[0]; p++) { x = x * 1103515245u + 12345u; *p = (uint8_t)(x >> 16); }
    for (i = 0; i < 16; i++) { x = x * 1103515245u + 12345u; w->poly[i] = (int16_t)((x >> 16) % (i & 1 ? H : W)); }
    for (k = 0; k < 2; k++) {
        w->bm[k].bytesperrow = BPR; w->bm[k].rows = H; w->bm[k].depth = 3;
        for (i = 0; i < 3; i++) w->bm[k].planes[i] = w->planes[k][i];
    }
    w->tbm.bytesperrow = BPR; w->tbm.rows = 2; w->tbm.depth = 3;
    for (i = 0; i < 3; i++) w->tbm.planes[i] = w->tplanes[i];
    gfx_InitRastPort(gfx_env_of(GB), &w->rp);
    gfx_InitRastPort(gfx_env_of(GB), &w->srp);
    gfx_InitRastPort(gfx_env_of(GB), &w->trp);
    w->rp.bitmap = &w->bm[0]; w->srp.bitmap = &w->bm[1]; w->trp.bitmap = &w->tbm;
    x = x * 1103515245u + 12345u;
    w->rp.fgpen = (int8_t)(x >> 16); w->rp.bgpen = (int8_t)(x >> 20);
    w->rp.drawmode = (int8_t)((x >> 24) % 3);
    w->rp.cp_x = (int16_t)((x >> 8) % W); w->rp.cp_y = (int16_t)((x >> 4) % H);
}

/* what can differ between A and B: the pixels, the arrays, the RastPorts'
 * state (Mask to PenHeight: pens, mode, pattern count, pen position) */
static int world_same(const struct world *a, const struct world *b)
{
    return !memcmp(a->planes, b->planes, sizeof a->planes) && !memcmp(a->tplanes, b->tplanes, sizeof a->tplanes) &&
           !memcmp(a->array, b->array, sizeof a->array) &&
           !memcmp((const uint8_t *)&a->rp + 24, (const uint8_t *)&b->rp + 24, 28) &&
           !memcmp((const uint8_t *)&a->srp + 24, (const uint8_t *)&b->srp + 24, 28);
}

/* ---- the provider and the look the test registers ---------------------------- */

#define NCALLS 22
/* each call's entry in struct OGFXProviderAll (v1's eight, then 1.4's) */
static const int slot_of[NCALLS] = { 1, 4, 5, 6, 0, 2, 7, 3, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21 };
static const int req_words[NCALLS] = { 4, 4, 4, 9, 5, 12, 8, 7, 7, 2, 3, 3, 4, 4, 10, 10, 9, 10, 10, 10, 10, 10 };
static const int result_word[NCALLS] = { 3, 3, -1, 8, -1, 11, -1, -1, -1, -1, -1, -1, 3, 3, -1, -1, -1, 9, 9, 9, 9, 9 };
static int call_of_slot[NCALLS];

/* the providers: five, one more than the chain holds; provider i's owner
 * is owner_p[i], its userdata &prov_ud[i], and take_of[i] the calls (by
 * slot) it takes. In the rules they are a, b, c, d and e. */
#define NPROV 5
static uint8_t owner_p[NPROV], prov_ud[NPROV], look_owner, look_owner2;
#define owner_a owner_p[0]
#define owner_b owner_p[1]
#define owner_c owner_p[2]
static U take_of[NPROV], look_take, round_no;
static void *const look_ud = (void *)0x10c4ed;
static int asked_from;                 /* the call under way */

static void print_req(const char *who, int slot, int taken, const U *r, int n)
{
    int i;
    printf("ASK %s %d %d", who, slot, taken);
    for (i = 0; i < n; i++) printf(" %08lx", (unsigned long)r[i]);
    printf("\n");
}

static LONG prov(int slot, APTR ud, void *req)
{
    int call = call_of_slot[slot];
    U *r = req;
    int p = (int)((uint8_t *)ud - prov_ud), taken, i;
    char who[4] = { 'P', 0, 0, 0 };
    if (p < 0 || p >= NPROV) { fail("provider userdata", slot); return 0; }
    taken = (take_of[p] >> slot) & 1;
    if (call != asked_from) fail("provider asked for another call", slot);
    if (quiet) {
        if (norder < (int)sizeof order - 1) order[norder++] = (char)('a' + p);
    } else {
        who[1] = (char)('0' + p);
        print_req(who, slot, taken, r, req_words[call]);
    }
    if (taken && result_word[call] >= 0)
        r[result_word[call]] = 0x12348000u + (U)p * 0x1000 + (U)slot * 16 + (round_no & 15);
    /* one that declines may write in the request: the next is given it as filled */
    if (!taken) for (i = 0; i < req_words[call]; i++) r[i] ^= 0x5a5a5a5au + (U)i;
    return taken ? 1 + (LONG)rn(5) : 0;              /* any non-zero is "mine" */
}
#define PROV(k) static LONG p##k(APTR ud, void *req) { return prov(k, ud, req); }
PROV(0) PROV(1) PROV(2) PROV(3) PROV(4) PROV(5) PROV(6) PROV(7) PROV(8) PROV(9) PROV(10)
PROV(11) PROV(12) PROV(13) PROV(14) PROV(15) PROV(16) PROV(17) PROV(18) PROV(19) PROV(20) PROV(21)
static LONG (*const prov_fn[NCALLS])(APTR, void *) = {
    p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21 };

/* the record, with room after it that must never be read */
static struct { struct OGFXProviderAll all; APTR after[4]; } rec;

static void make_record(ULONG size, U nullmask, int p)
{
    APTR *f = (APTR *)(void *)&rec.all.v1.rectfill;
    int k;
    for (k = 0; k < NCALLS; k++) f[k] = (nullmask >> k) & 1 ? NULL : (APTR)prov_fn[k];
    for (k = 0; k < 4; k++) rec.after[k] = (APTR)prov_fn[k];
    rec.all.v1.size = size;
    rec.all.v1.abi = OGFX_PROVIDER_ABI_V1;
    rec.all.v1.owner = &owner_p[p];
    rec.all.v1.userdata = &prov_ud[p];
}

static int look_mode;                  /* 0 the round's look; 1 re-entrant; 2 takes itself out */
static int look_depth;
static LONG look_rf(APTR ud, struct OGFXRectFillRequest *r)
{
    int taken = look_take & 1;
    if (ud != look_ud) fail("look userdata", 0);
    if (look_mode == 0) {
        if (asked_from != 4) fail("look asked for another call", asked_from);
        if (quiet) order[norder++] = 'L';
        else print_req("L", 0, taken, (const U *)(void *)r, 5);
        return taken ? 1 : 0;
    }
    /* the look draws its frame with RectFill itself, through the vector;
     * the call comes back here, and the look declines its own */
    printf("LOOK depth %d busy %d status %08lx\n", look_depth, (int)((uint16_t *)(void *)gfx_private_of(GB)->dispatch)[1],
           (unsigned long)gfx_lvo_GFX_DispatchStatus(GB));
    if (look_depth) return 0;
    look_depth++;
    if (look_mode == 2) {
        struct OGFXLookV1 l2 = { sizeof(struct OGFXLookV1), OGFX_LOOK_ABI_V1, &look_owner2, look_ud, 0, 0 };
        U out = gfx_lvo_GFX_UnregisterLook(&look_owner, GB);
        U in = gfx_lvo_GFX_RegisterLook(&l2, GB);
        U st = gfx_lvo_GFX_DispatchStatus(GB);
        printf("LOOKOUT %lu %lu %08lx\n", (unsigned long)out, (unsigned long)in, (unsigned long)st);
    }
    gfx_dispatch_RectFill((U)(uintptr_t)r->rp, (U)r->x0 + 1, (U)r->y0 + 1, (U)r->x1 - 1, (U)r->y1 - 1, GB);
    look_depth--;
    return 1;
}
static LONG look_tx(APTR ud, struct OGFXTextRequest *r)
{
    int taken = (look_take >> 1) & 1;
    if (ud != look_ud) fail("look userdata", 1);
    if (asked_from != 0) fail("look asked for another call", asked_from);
    if (quiet) order[norder++] = 'L';
    else print_req("L", 1, taken, (const U *)(void *)r, 4);
    if (taken) r->result = (LONG)(0x0bad0000u + round_no);
    return taken ? 1 : 0;
}

/* ---- one call, straight and dispatched ----------------------------------------- */

static U garbage(void) { return rn(3) ? rnd() << 16 : 0; }     /* the registers' upper words */

/* the call's registers, in graphics.lvo's order, for world w. wild: any
 * 32-bit values in the number registers (the provider takes them all, so
 * nothing draws) */
static int args_for(int c, struct world *w, U *a, int wild)
{
    U x0 = rn(W), y0 = rn(H), x1 = x0 + rn(W - x0), y1 = y0 + rn(H - y0);
    U bw = 1 + rn(W / 2), bh = 1 + rn(H / 2);
    U rp = (U)(uintptr_t)&w->rp, n;
#define G(v) ((v) | garbage())
#define RW(v) (wild ? rnd() : G(v))
    switch (c) {
    case 0: case 1: a[0] = rp; a[1] = (U)(uintptr_t)w->text; a[2] = RW(rn(40)); n = 3; break;
    case 2: a[0] = rp; a[1] = (U)(uintptr_t)w->text; a[2] = RW(rn(40)); a[3] = (U)(uintptr_t)w->te[0]; n = 4; break;
    case 3: a[0] = rp; a[1] = (U)(uintptr_t)w->text; a[2] = RW(rn(40)); a[3] = (U)(uintptr_t)w->te[0];
            a[4] = (U)(uintptr_t)w->te[1]; a[5] = rnd(); a[6] = rnd(); a[7] = rnd(); n = 8; break;
    case 4: a[0] = rp; a[1] = RW(x0); a[2] = RW(y0); a[3] = RW(x1); a[4] = RW(y1); n = 5; break;
    case 5: a[0] = (U)(uintptr_t)&w->bm[1]; a[1] = RW(rn(W - bw)); a[2] = RW(rn(H - bh)); a[3] = (U)(uintptr_t)&w->bm[0];
            a[4] = RW(rn(W - bw)); a[5] = RW(rn(H - bh)); a[6] = RW(bw); a[7] = RW(bh);
            a[8] = rn(2) ? G(0xC0) : rnd(); a[9] = rnd(); a[10] = 0; n = 11; break;
    case 6: a[0] = (U)(uintptr_t)w->mask; a[1] = rnd(); a[2] = rnd(); a[3] = rp; a[4] = rnd(); a[5] = rnd();
            a[6] = rnd(); a[7] = rnd(); n = 8; break;
    case 7: a[0] = rp; a[1] = RW(rn(17) - 8); a[2] = RW(rn(9) - 4); a[3] = RW(x0); a[4] = RW(y0); a[5] = RW(x1);
            a[6] = RW(y1); n = 7; break;
    case 8: a[0] = rp; a[1] = rn(2) ? (U)(uintptr_t)w->mask : 0; a[2] = RW(x0); a[3] = RW(y0); a[4] = RW(x1);
            a[5] = RW(y1); a[6] = RW(BPR); n = 7; break;
    case 9: a[0] = rp; a[1] = rnd(); n = 2; break;
    case 10: case 12: case 13: a[0] = rp; a[1] = RW(x0); a[2] = RW(y0); n = 3; break;
    case 11: a[0] = rp; a[1] = RW(rn(9)); a[2] = (U)(uintptr_t)w->poly; n = 3; break;
    case 14: a[0] = (U)(uintptr_t)&w->bm[1]; a[1] = RW(rn(W - bw)); a[2] = RW(rn(H - bh)); a[3] = rp;
             a[4] = RW(rn(W - bw)); a[5] = RW(rn(H - bh)); a[6] = RW(bw); a[7] = RW(bh);
             a[8] = rn(2) ? G(0xC0) : rnd(); n = 9; break;
    case 15: a[0] = (U)(uintptr_t)&w->bm[1]; a[1] = rnd(); a[2] = rnd(); a[3] = rp; a[4] = rnd(); a[5] = rnd();
             a[6] = rnd(); a[7] = rnd(); a[8] = rnd(); a[9] = (U)(uintptr_t)w->mask; n = 10; break;
    case 16: a[0] = (U)(uintptr_t)&w->srp; a[1] = RW(rn(W - bw)); a[2] = RW(rn(H - bh)); a[3] = rp;
             a[4] = RW(rn(W - bw)); a[5] = RW(rn(H - bh)); a[6] = RW(bw); a[7] = RW(bh);
             a[8] = rn(2) ? G(0xC0) : rnd(); n = 9; break;
    case 17: a[0] = rp; a[1] = RW(x0); a[2] = RW(y0); a[3] = RW(x1); a[4] = RW(y1); a[5] = (U)(uintptr_t)w->array;
             a[6] = wild ? rnd() : W; n = 7; break;
    case 18: case 21: a[0] = rp; a[1] = RW(x0); a[2] = RW(y0); a[3] = RW(x1); a[4] = RW(y1);
             a[5] = (U)(uintptr_t)w->array; a[6] = (U)(uintptr_t)&w->trp; n = 7; break;
    case 19: case 20: a[0] = rp; a[1] = RW(x0); a[2] = RW(y0); a[3] = RW(1 + rn(W - x0)); a[4] = (U)(uintptr_t)w->array;
             a[5] = (U)(uintptr_t)&w->trp; n = 6; break;
    default: n = 0;
    }
    return (int)n;
#undef G
#undef RW
}

/* the pointers of world A for those of world B */
static U to_a(U v)
{
    uintptr_t p = (uintptr_t)v, b = (uintptr_t)&wb;
    return p >= b && p < b + sizeof wb ? (U)(p - b + (uintptr_t)&wa) : v;
}

static U invoke(int c, int via, const U *a, void *gb)
{
    switch (c) {
    case 0: return via ? gfx_dispatch_Text(a[0], a[1], a[2], gb) : 0;
    case 1: return via ? (U)(int32_t)gfx_dispatch_TextLength(a[0], a[1], a[2], gb) : 0;
    case 2: if (via) gfx_dispatch_TextExtent(a[0], a[1], a[2], a[3], gb); return 0;
    case 3: return via ? gfx_dispatch_TextFit(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], gb) : 0;
    case 4: (via ? gfx_dispatch_RectFill : gfx_lvo_RectFill)(a[0], a[1], a[2], a[3], a[4], gb); return 0;
    case 5: return (via ? gfx_dispatch_BltBitMap : gfx_lvo_BltBitMap)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7],
                                                                        a[8], a[9], a[10], gb);
    case 6: if (via) gfx_dispatch_BltTemplate(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], gb); return 0;
    case 7: (via ? gfx_dispatch_ScrollRaster : gfx_lvo_ScrollRaster)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], gb); return 0;
    case 8: (via ? gfx_dispatch_BltPattern : gfx_lvo_BltPattern)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], gb); return 0;
    case 9: (via ? gfx_dispatch_SetRast : gfx_lvo_SetRast)(a[0], a[1], gb); return 0;
    case 10: (via ? gfx_dispatch_Draw : gfx_lvo_Draw)(a[0], a[1], a[2], gb); return 0;
    case 11: (via ? gfx_dispatch_PolyDraw : gfx_lvo_PolyDraw)(a[0], a[1], a[2], gb); return 0;
    case 12: return (via ? gfx_dispatch_WritePixel : gfx_lvo_WritePixel)(a[0], a[1], a[2], gb);
    case 13: return (via ? gfx_dispatch_ReadPixel : gfx_lvo_ReadPixel)(a[0], a[1], a[2], gb);
    case 14: (via ? gfx_dispatch_BltBitMapRastPort : gfx_lvo_BltBitMapRastPort)(a[0], a[1], a[2], a[3], a[4], a[5], a[6],
                                                                                  a[7], a[8], gb); return 0;
    case 15: if (via) gfx_dispatch_BltMaskBitMapRastPort(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], gb);
             return 0;
    case 16: (via ? gfx_dispatch_ClipBlit : gfx_lvo_ClipBlit)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], gb);
             return 0;
    case 17: (via ? gfx_dispatch_WriteChunkyPixels : gfx_lvo_WriteChunkyPixels)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], gb);
             return 0;
    case 18: return (via ? gfx_dispatch_WritePixelArray8 : gfx_lvo_WritePixelArray8)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], gb);
    case 19: return (via ? gfx_dispatch_WritePixelLine8 : gfx_lvo_WritePixelLine8)(a[0], a[1], a[2], a[3], a[4], a[5], gb);
    case 20: return (via ? gfx_dispatch_ReadPixelLine8 : gfx_lvo_ReadPixelLine8)(a[0], a[1], a[2], a[3], a[4], a[5], gb);
    case 21: return (via ? gfx_dispatch_ReadPixelArray8 : gfx_lvo_ReadPixelArray8)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], gb);
    }
    return 0;
}

static int is_face(int c) { return c != 0 && c != 1 && c != 2 && c != 3 && c != 6 && c != 15; }

/* C call n args...; the asks and the stand-in's AREA as they happen; then
 * R call result straight same untouched */
static void one_call(int c, int wild)
{
    U a[12] = { 0 }, aa[12] = { 0 }, r, rs = 0;
    uint32_t s = rnd();
    int n, i, same = 1, untouched = 1;
    world_fill(&wb, s);
    n = args_for(c, &wb, a, wild);
    printf("C %d %d", c, n);
    for (i = 0; i < n; i++) printf(" %08lx", (unsigned long)a[i]);
    printf("\n");
    asked_from = c;
    in_area = c;
    if (is_face(c)) {
        world_fill(&wa, s);
        world_fill(&pre, s);
        for (i = 0; i < n; i++) aa[i] = to_a(a[i]);
    }
    r = invoke(c, 1, a, GB);
    in_area = -1;
    if (is_face(c)) {
        int took = !world_same(&wb, &pre);
        untouched = !took;
        /* the straight call from the same start (never with wild values:
         * nothing may draw with those) */
        if (!wild) {
            rs = invoke(c, 0, aa, GB);
            same = world_same(&wa, &wb);
        }
    }
    asked_from = -1;
    printf("R %d %08lx %08lx %d %d\n", c, (unsigned long)r, (unsigned long)rs, same, untouched);
}

/* ---- the scenarios ------------------------------------------------------------- */

/* A scenario: np providers (np <= 4, all in the chain), each given as
 * size index (1 the 1.0 record, 2 v1, 3 1.4, 4 longer), nullmask, take,
 * priority and how (0 GFX_RegisterProvider, which is priority 0; 1
 * GFX_RegisterProviderPri), registered in that order; and the look.
 *   SCN np lm lnull ltake
 *   SP p size nullmask take pri how, then REGP p answer, for each */
struct sp { int pm; U nullmask, take; int pri, how; };
static int prov_in[NPROV];
static void scenario(int np, const struct sp *sp, int lm, U lnull)
{
    static const ULONG sizes[5] = { 0, OGFX_PROVIDER_V1_0_SIZE, sizeof(struct OGFXProviderV1),
                                    sizeof(struct OGFXProviderAll), sizeof(struct OGFXProviderAll) + 8 };
    struct OGFXLookV1 look;
    int p;
    for (p = 0; p < NPROV; p++)
        if (prov_in[p]) { gfx_lvo_GFX_UnregisterProvider(&owner_p[p], GB); prov_in[p] = 0; }
    gfx_lvo_GFX_UnregisterLook(&look_owner, GB);
    printf("SCN %d %d %08lx %08lx\n", np, lm, (unsigned long)lnull, (unsigned long)look_take);
    for (p = 0; p < np; p++) {
        U r;
        int pri = sp[p].how ? sp[p].pri : 0;
        take_of[p] = sp[p].take;
        printf("SP %d %lu %08lx %08lx %d %d\n", p, (unsigned long)sizes[sp[p].pm], (unsigned long)sp[p].nullmask,
               (unsigned long)sp[p].take, pri, sp[p].how);
        make_record(sizes[sp[p].pm], sp[p].nullmask, p);
        r = sp[p].how ? gfx_lvo_GFX_RegisterProviderPri(&rec.all.v1, (U)(LONG)pri, GB)
                      : gfx_lvo_GFX_RegisterProvider(&rec.all.v1, GB);
        printf("REGP %d %lu\n", p, (unsigned long)r);
        prov_in[p] = r != 0;
        memset(&rec, 0x55, sizeof rec);          /* the record is copied: the caller may reuse it */
    }
    if (lm) {
        look.size = sizeof look; look.abi = OGFX_LOOK_ABI_V1; look.owner = &look_owner; look.userdata = look_ud;
        look.rectfill = lnull & 1 ? NULL : look_rf;
        look.text = lnull & 2 ? NULL : look_tx;
        printf("REGL %lu\n", (unsigned long)gfx_lvo_GFX_RegisterLook(&look, GB));
        memset(&look, 0x55, sizeof look);
    }
    printf("ST %08lx\n", (unsigned long)gfx_lvo_GFX_DispatchStatus(GB));
}

static U take_any(void) { return rn(4) == 0 ? 0x3FFFFF : rn(4) == 0 ? 0 : rnd() & 0x3FFFFF; }

static void rounds(void)
{
    struct sp sp[GFXDISPATCH_PROVIDERS];
    int k, c;
    /* nothing registered, then chains of up to four providers of each
     * record size and priority, and a look, at random */
    for (round_no = 0; round_no < ROUNDS; round_no++) {
        int np = round_no < 2 ? 0 : (int)rn(GFXDISPATCH_PROVIDERS + 1), lm = round_no < 2 ? 0 : (int)rn(2);
        U lnull = rn(4) ? 0 : rn(4);
        for (k = 0; k < np; k++) {
            sp[k].pm = 1 + (int)rn(4);
            sp[k].nullmask = rn(3) ? 0 : rnd() & 0x3FFFFF;
            sp[k].take = take_any();
            sp[k].pri = rn(3) ? (int)rn(5) - 2 : (rn(2) ? 127 : -128);
            sp[k].how = (int)rn(2);
        }
        look_take = rn(4);
        look_mode = 0;
        scenario(np, sp, lm, lnull);
        for (c = 0; c < NCALLS; c++) one_call(c, 0);
        for (k = 0; k < 8; k++) one_call((int)rn(NCALLS), 0);
    }
    /* every call taken, with any value in every number register: the
     * first provider declines everything, the second takes everything */
    sp[0].pm = 3; sp[0].nullmask = 0; sp[0].take = 0; sp[0].pri = 5; sp[0].how = 1;
    sp[1].pm = 3; sp[1].nullmask = 0; sp[1].take = 0x3FFFFF; sp[1].pri = 0; sp[1].how = 0;
    look_take = 3;
    scenario(2, sp, 1, 0);
    for (k = 0; k < 4; k++)
        for (c = 0; c < NCALLS; c++) one_call(c, 1);
    look_take = 0;
    scenario(0, sp, 0, 0);
}

/* The chain's rules: call c through the dispatch, quiet, and print who was
 * asked in order (a to e the providers, L the look), who took it (- none)
 * and what it answered when taken.
 *   ORDER c asked taker answer */
static void order_of(int c)
{
    U a[12] = { 0 }, r;
    int taker;
    world_fill(&wb, 99);
    (void)args_for(c, &wb, a, 0);
    quiet = 1; norder = 0; asked_from = c; in_area = c;
    r = invoke(c, 1, a, GB);
    order[norder] = 0;
    quiet = 0; asked_from = -1; in_area = -1;
    taker = norder ? order[norder - 1] : '-';
    if (taker == 'L' ? !(look_take & (c == 4 ? 1 : 2)) : taker != '-' && !((take_of[taker - 'a'] >> slot_of[c]) & 1))
        taker = '-';
    if (taker != '-' && result_word[c] >= 0 && taker != 'L')
        printf("ORDER %d %s %c %08lx\n", c, norder ? order : "-", taker, (unsigned long)r);
    else
        printf("ORDER %d %s %c\n", c, norder ? order : "-", taker);
}

/* the registry's rules (gfx.h, and gfxdispatch.h's chain), line by line */
static void rules(void)
{
    struct OGFXLookV1 la = { sizeof(struct OGFXLookV1), OGFX_LOOK_ABI_V1, &look_owner, look_ud, look_rf, look_tx };
    struct OGFXLookV1 lb = { sizeof(struct OGFXLookV1), OGFX_LOOK_ABI_V1, &look_owner2, look_ud, look_rf, 0 };
    struct OGFXLookV1 bad;
    U a[12];
    int k;
    for (k = 0; k < NCALLS; k++) call_of_slot[slot_of[k]] = k;
    printf("VER %08lx\n", (unsigned long)gfx_lvo_GFX_DispatchVersion(GB));
    printf("ST %08lx\n", (unsigned long)gfx_lvo_GFX_DispatchStatus(GB));
    /* providers: refused records, then one, a second beside it, owners */
    make_record(sizeof(struct OGFXProviderAll), 0, 0);
    printf("PR null %lu\n", (unsigned long)gfx_lvo_GFX_RegisterProvider(NULL, GB));
    rec.all.v1.abi = 2;
    printf("PR abi2 %lu\n", (unsigned long)gfx_lvo_GFX_RegisterProvider(&rec.all.v1, GB));
    rec.all.v1.abi = OGFX_PROVIDER_ABI_V1; rec.all.v1.size = OGFX_PROVIDER_V1_0_SIZE - 4;
    printf("PR short %lu\n", (unsigned long)gfx_lvo_GFX_RegisterProvider(&rec.all.v1, GB));
    rec.all.v1.size = sizeof(struct OGFXProviderAll); rec.all.v1.owner = NULL;
    printf("PR noowner %lu\n", (unsigned long)gfx_lvo_GFX_RegisterProvider(&rec.all.v1, GB));
    printf("ST %08lx\n", (unsigned long)gfx_lvo_GFX_DispatchStatus(GB));
    rec.all.v1.owner = &owner_a;
    printf("PR a %lu\n", (unsigned long)gfx_lvo_GFX_RegisterProvider(&rec.all.v1, GB));
    make_record(sizeof(struct OGFXProviderV1), 0, 1);
    printf("PR b %lu\n", (unsigned long)gfx_lvo_GFX_RegisterProvider(&rec.all.v1, GB));
    printf("UP c %lu\n", (unsigned long)gfx_lvo_GFX_UnregisterProvider(&owner_c, GB));
    printf("UP null %lu\n", (unsigned long)gfx_lvo_GFX_UnregisterProvider(NULL, GB));
    printf("UP a %lu\n", (unsigned long)gfx_lvo_GFX_UnregisterProvider(&owner_a, GB));
    printf("ST %08lx\n", (unsigned long)gfx_lvo_GFX_DispatchStatus(GB));
    printf("UP b %lu\n", (unsigned long)gfx_lvo_GFX_UnregisterProvider(&owner_b, GB));
    printf("UP b %lu\n", (unsigned long)gfx_lvo_GFX_UnregisterProvider(&owner_b, GB));
    printf("ST %08lx\n", (unsigned long)gfx_lvo_GFX_DispatchStatus(GB));
    /* looks: refused records, one look, a second refused, owners */
    printf("LR null %lu\n", (unsigned long)gfx_lvo_GFX_RegisterLook(NULL, GB));
    bad = la; bad.abi = 2;
    printf("LR abi2 %lu\n", (unsigned long)gfx_lvo_GFX_RegisterLook(&bad, GB));
    bad = la; bad.size = sizeof bad - 4;
    printf("LR short %lu\n", (unsigned long)gfx_lvo_GFX_RegisterLook(&bad, GB));
    bad = la; bad.owner = NULL;
    printf("LR noowner %lu\n", (unsigned long)gfx_lvo_GFX_RegisterLook(&bad, GB));
    printf("LR a %lu\n", (unsigned long)gfx_lvo_GFX_RegisterLook(&la, GB));
    printf("LR b %lu\n", (unsigned long)gfx_lvo_GFX_RegisterLook(&lb, GB));
    printf("ST %08lx\n", (unsigned long)gfx_lvo_GFX_DispatchStatus(GB));
    printf("UL b %lu\n", (unsigned long)gfx_lvo_GFX_UnregisterLook(&look_owner2, GB));
    printf("UL null %lu\n", (unsigned long)gfx_lvo_GFX_UnregisterLook(NULL, GB));
    /* a look that draws its frame with RectFill: the RectFill comes back
     * through the dispatch to the look, which declines it, and the face draws */
    look_mode = 1;
    world_fill(&wb, 7);
    world_fill(&wa, 7);
    wb.rp.drawmode = wa.rp.drawmode = 1;
    wb.rp.fgpen = wa.rp.fgpen = 5;
    asked_from = 4;
    in_area = -1;
    gfx_dispatch_RectFill((U)(uintptr_t)&wb.rp, 10, 4, 30, 20, GB);
    gfx_lvo_RectFill((U)(uintptr_t)&wa.rp, 11, 5, 29, 19, GB);
    printf("REENTER same %d busy %d\n", world_same(&wa, &wb), (int)((uint16_t *)(void *)gfx_private_of(GB)->dispatch)[1]);
    printf("ST %08lx\n", (unsigned long)gfx_lvo_GFX_DispatchStatus(GB));
    /* a look that takes itself out while it draws: it is out at once, a new
     * look is refused while the call is inside, and taken once it is done */
    look_mode = 2;
    world_fill(&wb, 8);
    world_fill(&wa, 8);
    gfx_dispatch_RectFill((U)(uintptr_t)&wb.rp, 0, 0, 63, 31, GB);
    gfx_lvo_RectFill((U)(uintptr_t)&wa.rp, 1, 1, 62, 30, GB);
    printf("OUT same %d\n", world_same(&wa, &wb));
    printf("ST %08lx\n", (unsigned long)gfx_lvo_GFX_DispatchStatus(GB));
    printf("LR b %lu\n", (unsigned long)gfx_lvo_GFX_RegisterLook(&lb, GB));
    printf("UL a %lu\n", (unsigned long)gfx_lvo_GFX_UnregisterLook(&look_owner, GB));
    printf("UL b %lu\n", (unsigned long)gfx_lvo_GFX_UnregisterLook(&look_owner2, GB));
    printf("ST %08lx\n", (unsigned long)gfx_lvo_GFX_DispatchStatus(GB));
    /* the provider chain (47.102): a to e are providers 0 to 4, each with
     * every call (a 1.4 record) and taking nothing unless said */
    look_mode = 0;
    look_take = 0;
    for (k = 0; k < NPROV; k++) take_of[k] = 0;
    make_record(sizeof(struct OGFXProviderAll), 0, 0);
    printf("PR a %lu\n", (unsigned long)gfx_lvo_GFX_RegisterProvider(&rec.all.v1, GB));
    make_record(sizeof(struct OGFXProviderAll), 0, 1);
    printf("PRP b 10 %lu\n", (unsigned long)gfx_lvo_GFX_RegisterProviderPri(&rec.all.v1, 10, GB));
    make_record(sizeof(struct OGFXProviderAll), 0, 2);
    printf("PRP c -10 %lu\n", (unsigned long)gfx_lvo_GFX_RegisterProviderPri(&rec.all.v1, (U)-10, GB));
    make_record(sizeof(struct OGFXProviderAll), 0, 3);
    printf("PRP d 10 %lu\n", (unsigned long)gfx_lvo_GFX_RegisterProviderPri(&rec.all.v1, 10, GB));
    memset(&rec, 0x55, sizeof rec);
    printf("ST %08lx\n", (unsigned long)gfx_lvo_GFX_DispatchStatus(GB));
    order_of(4);                                 /* by priority, equals as they came: b d a c */
    make_record(sizeof(struct OGFXProviderAll), 0, 4);
    printf("PRP e 0 %lu\n", (unsigned long)gfx_lvo_GFX_RegisterProviderPri(&rec.all.v1, 0, GB));   /* full */
    order_of(4);
    /* a decline falls through to the next; the one that takes it ends the call */
    take_of[3] = 0x3FFFFF;
    order_of(4);
    order_of(13);
    take_of[3] = 0;
    /* the same owner again: its own entry, at its new priority, not another's */
    make_record(sizeof(struct OGFXProviderAll), 0, 0);
    printf("PRP a 20 %lu\n", (unsigned long)gfx_lvo_GFX_RegisterProviderPri(&rec.all.v1, 20, GB));
    order_of(4);
    /* again at the same priority: its place kept, before d (also 10) */
    make_record(sizeof(struct OGFXProviderAll), 0, 1);
    printf("PRP b 10 %lu\n", (unsigned long)gfx_lvo_GFX_RegisterProviderPri(&rec.all.v1, 10, GB));
    order_of(4);
    /* again at the same priority, a 1.0 record (four calls): its place kept */
    make_record(OGFX_PROVIDER_V1_0_SIZE, 0, 3);
    printf("PRP d 10 %lu\n", (unsigned long)gfx_lvo_GFX_RegisterProviderPri(&rec.all.v1, 10, GB));
    make_record(sizeof(struct OGFXProviderV1), 0, 2);     /* and c a v1 record (eight calls) */
    printf("PRP c -10 %lu\n", (unsigned long)gfx_lvo_GFX_RegisterProviderPri(&rec.all.v1, (U)-10, GB));
    memset(&rec, 0x55, sizeof rec);
    order_of(4);                                 /* RectFill: all four records have it */
    order_of(1);                                 /* TextLength: not d's 1.0 record */
    order_of(13);                                /* ReadPixel: neither d's nor c's */
    take_of[1] = 1u << slot_of[1];               /* b takes TextLength: c not asked */
    order_of(1);
    take_of[1] = 0;
    /* one owner out, the others as they were; a NULL entry not asked */
    printf("UP d %lu\n", (unsigned long)gfx_lvo_GFX_UnregisterProvider(&owner_p[3], GB));
    printf("UP d %lu\n", (unsigned long)gfx_lvo_GFX_UnregisterProvider(&owner_p[3], GB));
    printf("ST %08lx\n", (unsigned long)gfx_lvo_GFX_DispatchStatus(GB));
    order_of(4);
    make_record(sizeof(struct OGFXProviderAll), 1u << slot_of[4], 4);
    printf("PRP e -10 %lu\n", (unsigned long)gfx_lvo_GFX_RegisterProviderPri(&rec.all.v1, (U)-10, GB));
    memset(&rec, 0x55, sizeof rec);
    order_of(4);                                 /* e's RectFill is NULL */
    order_of(6);                                 /* BltTemplate: e after c, as it came after */
    /* the look still first */
    printf("LR a %lu\n", (unsigned long)gfx_lvo_GFX_RegisterLook(&la, GB));
    order_of(4);
    order_of(0);
    look_take = 1;
    order_of(4);
    look_take = 0;
    printf("UL a %lu\n", (unsigned long)gfx_lvo_GFX_UnregisterLook(&look_owner, GB));
    for (k = 0; k < NPROV; k++)
        printf("UP %c %lu\n", 'a' + k, (unsigned long)gfx_lvo_GFX_UnregisterProvider(&owner_p[k], GB));
    printf("ST %08lx\n", (unsigned long)gfx_lvo_GFX_DispatchStatus(GB));
    order_of(4);
    look_mode = 0;
    asked_from = -1;
    (void)a;
}

int main(void)
{
    struct gfx_env *e = gfx_env_of(GB);
    e->ctx = GB;
    e->base = GB;
    e->alloc = t_alloc; e->free = t_free;
    e->forbid = t_forbid; e->permit = t_permit; e->disable = e->enable = t_nop;
    e->sem_init = t_sem; e->sem_obtain = e->sem_obtain_shared = t_sem; e->sem_release = t_sem;
    e->sem_attempt = t_sem_attempt;
    memset(gfx_private_of(GB)->dispatch, 0xA5, GFX_DISPATCH_BYTES);
    gfx_clear_dispatch(gfx_private_of(GB));
    rules();
    rounds();
    printf("FORBID %d %d %d\n", forbids, permits, forbid_depth);
    printf("END %d\n", errors);
    return errors != 0;
}
