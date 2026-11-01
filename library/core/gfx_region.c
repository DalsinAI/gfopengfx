/* SPDX-License-Identifier: MIT */
/* OpenGfx's regions (gfx_region.h). Copyright (c) 2026 AmigaChrome Team. MIT.
 *
 * Every call is a sweep: the edges of both operands cut the plane into
 * bands; in each band the two operands are runs of x, combined by the
 * operation; a band whose runs equal the band just above it grows that
 * band instead of starting a new one. Work is in half-open int32 spans so
 * that MaxX + 1 of 32767 needs no care.
 *
 * With a rectangle, only the region's bands from the first it meets to the
 * last are swept again, with the band just above and just below when they
 * touch those (the answer may join them); the rest stays as it is. OR of a
 * rectangle the region already holds changes nothing, and OR of one below
 * the last band adds it at the end. An answer is built in work space, then
 * written over the rectangles it replaces: only those it needs beyond them
 * are allocated, before anything changes (so running out of memory leaves
 * the region as it was), and those left over are freed. */
#include "gfx_region.h"

struct span { int32_t x0, x1; };
struct box { int32_t x0, y0, x1, y1; };

/* An answer being built: its rectangles top to bottom, in bands */
struct out {
    struct box *r;
    uint32_t n, cap;
    uint32_t band;          /* where the last band starts in r */
    int banded;             /* there is a last band */
    int grows;              /* r may be made bigger; else cap is enough by reckoning */
};

/* The extent of no rectangles */
static const struct box nothing = { INT32_MAX, INT32_MAX, INT32_MIN, INT32_MIN };

void gfx_rgn_init(struct gfx_region *r)
{
    r->bounds.min_x = r->bounds.min_y = r->bounds.max_x = r->bounds.max_y = 0;
    r->rects = 0;
}

static void free_list(struct gfx_rrect *p, const struct gfx_rgn_mem *m)
{
    while (p) {
        struct gfx_rrect *n = p->next;
        m->free(m->ctx, p, sizeof *p);
        p = n;
    }
}

void gfx_rgn_clear(struct gfx_region *r, const struct gfx_rgn_mem *m)
{
    free_list(r->rects, m);
    gfx_rgn_init(r);
}

static uint32_t count(const struct gfx_region *r)
{
    const struct gfx_rrect *p;
    uint32_t n = 0;
    for (p = r->rects; p; p = p->next) n++;
    return n;
}

/* A rectangle's coordinates are offsets from the region's MinX and MinY,
 * 0 to 65535: a region may be 65536 wide, so they are read unsigned. */
static uint32_t boxes(const struct gfx_region *r, struct box *b)
{
    const struct gfx_rrect *p;
    uint32_t n = 0;
    for (p = r->rects; p; p = p->next) {
        b[n].x0 = (int32_t)r->bounds.min_x + (uint16_t)p->bounds.min_x;
        b[n].y0 = (int32_t)r->bounds.min_y + (uint16_t)p->bounds.min_y;
        b[n].x1 = (int32_t)r->bounds.min_x + (uint16_t)p->bounds.max_x + 1;
        b[n].y1 = (int32_t)r->bounds.min_y + (uint16_t)p->bounds.max_y + 1;
        if (b[n].x1 > b[n].x0 && b[n].y1 > b[n].y0) n++;
    }
    return n;
}

static void sort_ints(int32_t *a, uint32_t n)
{
    uint32_t gap, i, j;
    for (gap = n / 2; gap; gap /= 2)
        for (i = gap; i < n; i++) {
            int32_t v = a[i];
            for (j = i; j >= gap && a[j - gap] > v; j -= gap) a[j] = a[j - gap];
            a[j] = v;
        }
}

/* The runs of x that the boxes cover through the band y0..y1, sorted and
 * merged where they overlap or touch. */
static uint32_t runs(const struct box *b, uint32_t n, int32_t y0, int32_t y1, struct span *s)
{
    uint32_t i, j, k = 0;
    for (i = 0; i < n; i++)
        if (b[i].y0 <= y0 && b[i].y1 >= y1) {
            struct span v;
            v.x0 = b[i].x0;
            v.x1 = b[i].x1;
            for (j = k; j > 0 && s[j - 1].x0 > v.x0; j--) s[j] = s[j - 1];
            s[j] = v;
            k++;
        }
    if (!k) return 0;
    for (i = 1, j = 0; i < k; i++)
        if (s[i].x0 <= s[j].x1) {
            if (s[i].x1 > s[j].x1) s[j].x1 = s[i].x1;
        } else
            s[++j] = s[i];
    return j + 1;
}

static int in(int op, int a, int b)
{
    switch (op) {
    case GFX_RGN_OR: return a || b;
    case GFX_RGN_AND: return a && b;
    case GFX_RGN_XOR: return a != b;
    default: return a && !b;    /* CLEAR: dst less the rectangle */
    }
}

/* Two sorted lists of runs combined by op, into o; e is room for their
 * edges. */
static uint32_t combine(int op, const struct span *a, uint32_t na, const struct span *b, uint32_t nb,
                        int32_t *e, struct span *o)
{
    uint32_t ia = 0, ib = 0, ne = 0, k, n = 0;
    /* both lists' edges, in order, each once */
    while (ia < 2 * na || ib < 2 * nb) {
        int32_t va = ia < 2 * na ? (ia & 1 ? a[ia >> 1].x1 : a[ia >> 1].x0) : INT32_MAX;
        int32_t vb = ib < 2 * nb ? (ib & 1 ? b[ib >> 1].x1 : b[ib >> 1].x0) : INT32_MAX;
        int32_t v = va < vb ? va : vb;
        if (va == v) ia++;
        if (vb == v) ib++;
        if (!ne || e[ne - 1] != v) e[ne++] = v;
    }
    ia = ib = 0;
    for (k = 0; k + 1 < ne; k++) {
        int32_t x = e[k];
        while (ia < na && a[ia].x1 <= x) ia++;
        while (ib < nb && b[ib].x1 <= x) ib++;
        if (!in(op, ia < na && a[ia].x0 <= x, ib < nb && b[ib].x0 <= x)) continue;
        if (n && o[n - 1].x1 == x)
            o[n - 1].x1 = e[k + 1];
        else {
            o[n].x0 = x;
            o[n].x1 = e[k + 1];
            n++;
        }
    }
    return n;
}

/* ---- Answers, and writing them over the rectangles they replace ---------- */

/* Room for need rectangles in o */
static int grow(struct out *o, uint32_t need, const struct gfx_rgn_mem *m)
{
    uint32_t cap = o->cap * 2, i;
    struct box *r;
    if (!o->grows) return 0;
    if (cap < need) cap = need;
    if (cap < 16) cap = 16;
    r = (struct box *)m->alloc(m->ctx, cap * (uint32_t)sizeof(struct box));
    if (!r) return 0;
    for (i = 0; i < o->n; i++) r[i] = o->r[i];
    if (o->r) m->free(m->ctx, o->r, o->cap * (uint32_t)sizeof(struct box));
    o->r = r;
    o->cap = cap;
    return 1;
}

/* A band's k runs s through rows y0..y1 onto the answer: the band just
 * above grows instead when it touches and holds the same runs. */
static int put_band(struct out *o, const struct span *s, uint32_t k, int32_t y0, int32_t y1,
                    const struct gfx_rgn_mem *m)
{
    uint32_t j;
    if (!k) return 1;
    if (o->banded && o->r[o->band].y1 == y0 && o->n - o->band == k) {
        for (j = 0; j < k && o->r[o->band + j].x0 == s[j].x0 && o->r[o->band + j].x1 == s[j].x1; j++) ;
        if (j == k) {
            for (j = 0; j < k; j++) o->r[o->band + j].y1 = y1;
            return 1;
        }
    }
    if (o->n + k > o->cap && !grow(o, o->n + k, m)) return 0;
    o->band = o->n;
    o->banded = 1;
    for (j = 0; j < k; j++, o->n++) {
        o->r[o->n].x0 = s[j].x0;
        o->r[o->n].y0 = y0;
        o->r[o->n].x1 = s[j].x1;
        o->r[o->n].y1 = y1;
    }
    return 1;
}

static void extend(struct box *e, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    if (x0 < e->x0) e->x0 = x0;
    if (y0 < e->y0) e->y0 = y0;
    if (x1 > e->x1) e->x1 = x1;
    if (y1 > e->y1) e->y1 = y1;
}

/* A rectangle measured from the corner nx, ny instead of ox, oy */
static void rebase(struct gfx_rrect *p, int32_t ox, int32_t oy, int32_t nx, int32_t ny)
{
    p->bounds.min_x = (int16_t)(ox + (uint16_t)p->bounds.min_x - nx);
    p->bounds.min_y = (int16_t)(oy + (uint16_t)p->bounds.min_y - ny);
    p->bounds.max_x = (int16_t)(ox + (uint16_t)p->bounds.max_x - nx);
    p->bounds.max_y = (int16_t)(oy + (uint16_t)p->bounds.max_y - ny);
}

/* The answer o written over the count rectangles of dst after before
 * (NULL: from the first); the others stay where they are, and outside is
 * their extent. Only the rectangles o needs beyond count are allocated,
 * before anything changes; those left over are freed. */
static int commit(struct gfx_region *dst, struct gfx_rrect *before, uint32_t count, const struct out *o,
                  const struct box *outside, const struct gfx_rgn_mem *m)
{
    struct gfx_rrect *more = 0, *first = before ? before->next : dst->rects, *after = first, *p, *q, *last;
    int32_t ox = dst->bounds.min_x, oy = dst->bounds.min_y;
    struct box e = *outside;
    uint32_t i;
    for (i = count; i < o->n; i++) {
        p = (struct gfx_rrect *)m->alloc(m->ctx, sizeof *p);
        if (!p) {
            free_list(more, m);
            return 0;
        }
        p->next = more;
        more = p;
    }
    for (i = 0; i < count; i++) after = after->next;
    for (i = 0; i < o->n; i++) extend(&e, o->r[i].x0, o->r[i].y0, o->r[i].x1, o->r[i].y1);
    if (e.x1 <= e.x0) {
        /* nothing left (and nothing was outside: first is the whole list) */
        free_list(first, m);
        gfx_rgn_init(dst);
        return 1;
    }
    if (e.x0 != ox || e.y0 != oy) {
        /* the corner moves: the rectangles that stay are measured from the new one */
        for (p = dst->rects; p != first; p = p->next) rebase(p, ox, oy, e.x0, e.y0);
        for (p = after; p; p = p->next) rebase(p, ox, oy, e.x0, e.y0);
    }
    last = before;
    p = first;
    for (i = 0; i < o->n; i++) {
        if (i < count) {
            q = p;
            p = p->next;
        } else {
            q = more;
            more = more->next;
        }
        q->bounds.min_x = (int16_t)(o->r[i].x0 - e.x0);
        q->bounds.min_y = (int16_t)(o->r[i].y0 - e.y0);
        q->bounds.max_x = (int16_t)(o->r[i].x1 - 1 - e.x0);
        q->bounds.max_y = (int16_t)(o->r[i].y1 - 1 - e.y0);
        q->prev = last;
        if (last) last->next = q; else dst->rects = q;
        last = q;
    }
    for (; i < count; i++) {
        q = p;
        p = p->next;
        m->free(m->ctx, q, sizeof *q);
    }
    if (last) last->next = after; else dst->rects = after;
    if (after) after->prev = last;
    dst->bounds.min_x = (int16_t)e.x0;
    dst->bounds.min_y = (int16_t)e.y0;
    dst->bounds.max_x = (int16_t)(e.x1 - 1);
    dst->bounds.max_y = (int16_t)(e.y1 - 1);
    return 1;
}

/* ---- Any list: the whole sweep -------------------------------------------- */

/* dst's answer for op with the boxes b of the other operand. */
static int sweep(struct gfx_region *dst, int op, const struct box *b, uint32_t nb, const struct gfx_rgn_mem *m)
{
    uint32_t na = count(dst), nold = na, i, ny = 0;
    uint32_t wsize = (na + nb) * (uint32_t)(sizeof(struct box) + 4 * sizeof(int32_t) + 3 * sizeof(struct span))
                     + 8;
    struct box *a;
    int32_t *ys, *edges;
    struct span *sa, *sb, *so;
    struct out o;
    int ok = 1;
    void *w = m->alloc(m->ctx, wsize);
    if (!w) return 0;
    a = (struct box *)w;
    ys = (int32_t *)(a + na + nb);
    sa = (struct span *)(ys + 2 * (na + nb));
    sb = sa + na + nb;
    so = sb + na + nb;
    edges = (int32_t *)(so + na + nb);
    na = boxes(dst, a);
    for (i = 0; i < na; i++) { ys[ny++] = a[i].y0; ys[ny++] = a[i].y1; }
    for (i = 0; i < nb; i++) { ys[ny++] = b[i].y0; ys[ny++] = b[i].y1; }
    sort_ints(ys, ny);
    o.r = 0;
    o.n = o.cap = o.band = 0;
    o.banded = 0;
    o.grows = 1;
    for (i = 0; ok && i + 1 < ny; i++) {
        uint32_t ka, kb, ko;
        if (ys[i] == ys[i + 1]) continue;
        ka = runs(a, na, ys[i], ys[i + 1], sa);
        kb = runs(b, nb, ys[i], ys[i + 1], sb);
        ko = combine(op, sa, ka, sb, kb, edges, so);
        ok = put_band(&o, so, ko, ys[i], ys[i + 1], m);
    }
    m->free(m->ctx, w, wsize);
    if (ok) ok = commit(dst, 0, nold, &o, &nothing, m);
    if (o.r) m->free(m->ctx, o.r, o.cap * (uint32_t)sizeof(struct box));
    return ok;
}

/* ---- A rectangle on a region kept in bands -------------------------------- */

static int same_runs(const struct gfx_rrect *a, uint32_t na, const struct gfx_rrect *b, uint32_t nb)
{
    if (na != nb) return 0;
    for (; na; na--, a = a->next, b = b->next)
        if (a->bounds.min_x != b->bounds.min_x || a->bounds.max_x != b->bounds.max_x) return 0;
    return 1;
}

/* 1 when r is kept the way this file keeps a region (gfx_region.h), as its
 * own answers always are: bands top to bottom, runs left to right that
 * don't touch, touching bands with different runs, tight bounds, the list
 * linked both ways. Another list (one made by hand) goes through sweep. */
static int banded(const struct gfx_region *r)
{
    const struct gfx_rrect *p = r->rects, *prev = 0, *above = 0;
    int32_t w = (int32_t)r->bounds.max_x - r->bounds.min_x, h = (int32_t)r->bounds.max_y - r->bounds.min_y;
    int32_t above_y1 = -1, x0 = 65536, x1 = -1;
    uint32_t above_n = 0;
    if (!p) return !r->bounds.min_x && !r->bounds.min_y && !r->bounds.max_x && !r->bounds.max_y;
    if (p->bounds.min_y) return 0;
    while (p) {
        const struct gfx_rrect *band = p;
        int32_t y0 = (uint16_t)p->bounds.min_y, y1 = (uint16_t)p->bounds.max_y, x = -2;
        uint32_t n = 0;
        if (y1 < y0 || y0 <= above_y1) return 0;
        for (; p && p->bounds.min_y == band->bounds.min_y; prev = p, p = p->next, n++) {
            int32_t px0 = (uint16_t)p->bounds.min_x, px1 = (uint16_t)p->bounds.max_x;
            if (p->prev != prev || p->bounds.max_y != band->bounds.max_y || px0 <= x + 1 || px1 < px0) return 0;
            if (px0 < x0) x0 = px0;
            if (px1 > x1) x1 = px1;
            x = px1;
        }
        if (above && y0 == above_y1 + 1 && same_runs(above, above_n, band, n)) return 0;
        above = band;
        above_n = n;
        above_y1 = y1;
    }
    return x0 == 0 && x1 == w && above_y1 == h;
}

/* 1 when b is all inside r */
static int covers(const struct gfx_region *r, const struct box *b)
{
    const struct gfx_rrect *p = r->rects;
    int32_t ox = r->bounds.min_x, oy = r->bounds.min_y, y = b->y0;
    while (p && y < b->y1) {
        const struct gfx_rrect *band = p;
        int32_t y0 = oy + (uint16_t)p->bounds.min_y, y1 = oy + (uint16_t)p->bounds.max_y + 1;
        int holds = 0;
        for (; p && p->bounds.min_y == band->bounds.min_y; p = p->next)
            if (ox + (uint16_t)p->bounds.min_x <= b->x0 && ox + (uint16_t)p->bounds.max_x + 1 >= b->x1) holds = 1;
        if (y1 <= y) continue;              /* above it */
        if (y0 > y || !holds) return 0;     /* a gap, or a band without it */
        y = y1;
    }
    return y >= b->y1;
}

/* OR of b below r's last band (or into an empty r): the last band grows
 * when it touches b and holds b's run alone; else b is a band of its own. */
static int append(struct gfx_region *r, const struct box *b, const struct gfx_rgn_mem *m)
{
    struct gfx_rrect *tail = r->rects, *n, *p;
    int32_t ox = r->bounds.min_x, oy = r->bounds.min_y, x0 = b->x0, x1 = b->x1;
    if (tail) {
        while (tail->next) tail = tail->next;
        if (oy + (uint16_t)tail->bounds.max_y + 1 == b->y0
            && (!tail->prev || tail->prev->bounds.min_y != tail->bounds.min_y)
            && ox + (uint16_t)tail->bounds.min_x == b->x0 && ox + (uint16_t)tail->bounds.max_x + 1 == b->x1) {
            tail->bounds.max_y = (int16_t)(b->y1 - 1 - oy);
            r->bounds.max_y = (int16_t)(b->y1 - 1);
            return 1;
        }
    }
    n = (struct gfx_rrect *)m->alloc(m->ctx, sizeof *n);
    if (!n) return 0;
    if (tail) {
        if (ox < x0) x0 = ox;
        if (r->bounds.max_x + 1 > x1) x1 = r->bounds.max_x + 1;
        if (x0 != ox)
            for (p = r->rects; p; p = p->next) rebase(p, ox, oy, x0, oy);
    } else {
        oy = b->y0;
        r->bounds.min_y = (int16_t)oy;
    }
    n->bounds.min_x = (int16_t)(b->x0 - x0);
    n->bounds.min_y = (int16_t)(b->y0 - oy);
    n->bounds.max_x = (int16_t)(b->x1 - 1 - x0);
    n->bounds.max_y = (int16_t)(b->y1 - 1 - oy);
    n->next = 0;
    n->prev = tail;
    if (tail) tail->next = n; else r->rects = n;
    r->bounds.min_x = (int16_t)x0;
    r->bounds.max_x = (int16_t)(x1 - 1);
    r->bounds.max_y = (int16_t)(b->y1 - 1);
    return 1;
}

/* The bands a sweep of rows y0..y1 takes in */
struct walk {
    struct gfx_rrect *before;   /* the last rectangle above them; NULL: they start the list */
    uint32_t count, bands, most;    /* their rectangles, their bands, the most runs a band has */
    struct box outside;         /* the extent of the rest */
};

/* The bands rows y0..y1 cross (each whole), and the band just above and
 * just below when one touches those; with all, every band. */
static void locate(const struct gfx_region *r, int32_t y0, int32_t y1, int all, struct walk *w)
{
    struct gfx_rrect *p;
    int32_t ox = r->bounds.min_x, oy = r->bounds.min_y, top = y0, bottom = y1;
    for (p = r->rects; p; ) {
        struct gfx_rrect *band = p;
        int32_t by0 = oy + (uint16_t)p->bounds.min_y, by1 = oy + (uint16_t)p->bounds.max_y + 1;
        if (by0 >= y1) break;
        while (p && p->bounds.min_y == band->bounds.min_y) p = p->next;
        if (by1 > y0) {
            if (by0 < top) top = by0;
            if (by1 > bottom) bottom = by1;
        }
    }
    w->before = 0;
    w->count = w->bands = w->most = 0;
    w->outside = nothing;
    for (p = r->rects; p; ) {
        struct gfx_rrect *band = p, *last = p;
        int32_t by0 = oy + (uint16_t)p->bounds.min_y, by1 = oy + (uint16_t)p->bounds.max_y + 1;
        uint32_t n = 0;
        for (; p && p->bounds.min_y == band->bounds.min_y; last = p, p = p->next) n++;
        if (all || (by1 >= top && by0 <= bottom)) {
            w->count += n;
            w->bands++;
            if (n > w->most) w->most = n;
        } else {
            if (by1 < top) w->before = last;
            extend(&w->outside, ox + (uint16_t)band->bounds.min_x, by0, ox + (uint16_t)last->bounds.max_x + 1, by1);
        }
    }
}

/* dst op b over the bands w takes in, swept again: the rest of dst stays.
 * Through B bands of T rectangles, b cuts at most two bands and fills at
 * most B + 3 gaps, and a band's k runs with b's one make at most k + 1, so
 * the answer has at most 3T + 2B + 5 rectangles. */
static int redo(struct gfx_region *dst, int op, const struct box *b, const struct walk *w,
                const struct gfx_rgn_mem *m)
{
    uint32_t k = w->most + 1, nys = 2 * w->bands + 2, cap = 3 * w->count + 2 * w->bands + 5, ny = 0, left, i, ka, ko;
    uint32_t size = cap * (uint32_t)sizeof(struct box) + (nys + 2 * k) * (uint32_t)sizeof(int32_t)
                    + 2 * k * (uint32_t)sizeof(struct span);
    struct gfx_rrect *first = w->before ? w->before->next : dst->rects, *p, *q;
    int32_t ox = dst->bounds.min_x, oy = dst->bounds.min_y, *ys, *edges;
    struct span *sa, *so, sb;
    struct out o;
    int ok = 1;
    void *work = m->alloc(m->ctx, size);
    if (!work) return 0;
    o.r = (struct box *)work;
    o.n = o.band = 0;
    o.cap = cap;
    o.banded = 0;
    o.grows = 0;
    ys = (int32_t *)(o.r + cap);
    edges = ys + nys;
    sa = (struct span *)(edges + 2 * k);
    so = sa + k;
    for (p = first, left = w->count; left; ) {
        q = p;
        ys[ny++] = oy + (uint16_t)q->bounds.min_y;
        ys[ny++] = oy + (uint16_t)q->bounds.max_y + 1;
        for (; left && p->bounds.min_y == q->bounds.min_y; left--) p = p->next;
    }
    ys[ny++] = b->y0;
    ys[ny++] = b->y1;
    sort_ints(ys, ny);
    sb.x0 = b->x0;
    sb.x1 = b->x1;
    for (p = first, left = w->count, i = 0; ok && i + 1 < ny; i++) {
        int32_t ya = ys[i], yb = ys[i + 1];
        if (ya == yb) continue;
        /* past the bands above these rows; then the band through them, if one is */
        while (left && oy + (uint16_t)p->bounds.max_y + 1 <= ya)
            for (q = p; left && p->bounds.min_y == q->bounds.min_y; left--) p = p->next;
        ka = 0;
        if (left && oy + (uint16_t)p->bounds.min_y <= ya)
            for (q = p; ka < left && q->bounds.min_y == p->bounds.min_y; q = q->next, ka++) {
                sa[ka].x0 = ox + (uint16_t)q->bounds.min_x;
                sa[ka].x1 = ox + (uint16_t)q->bounds.max_x + 1;
            }
        ko = combine(op, sa, ka, &sb, b->y0 <= ya && b->y1 >= yb, edges, so);
        ok = put_band(&o, so, ko, ya, yb, m);
    }
    if (ok) ok = commit(dst, w->before, w->count, &o, &w->outside, m);
    m->free(m->ctx, work, size);
    return ok;
}

/* ---- The calls --------------------------------------------------------------- */

int gfx_rgn_rect(struct gfx_region *dst, int op, const struct gfx_rect *rect, const struct gfx_rgn_mem *m)
{
    struct box b;
    struct walk w;
    uint32_t n = 0;
    if (rect->max_x >= rect->min_x && rect->max_y >= rect->min_y) {
        b.x0 = rect->min_x;
        b.y0 = rect->min_y;
        b.x1 = (int32_t)rect->max_x + 1;
        b.y1 = (int32_t)rect->max_y + 1;
        n = 1;
    }
    if (!banded(dst)) return sweep(dst, op, &b, n, m);
    if (!n) {
        /* nothing to add, keep or take away: AND empties the region */
        if (op == GFX_RGN_AND) gfx_rgn_clear(dst, m);
        return 1;
    }
    if (op == GFX_RGN_OR) {
        if (dst->rects && covers(dst, &b)) return 1;
        if (!dst->rects || b.y0 > dst->bounds.max_y) return append(dst, &b, m);
    }
    locate(dst, b.y0, b.y1, op == GFX_RGN_AND, &w);
    return redo(dst, op, &b, &w, m);
}

int gfx_rgn_region(struct gfx_region *dst, int op, const struct gfx_region *src, const struct gfx_rgn_mem *m)
{
    uint32_t n = count(src);
    uint32_t size = n * (uint32_t)sizeof(struct box) + 4;
    struct box *b = (struct box *)m->alloc(m->ctx, size);
    int ok;
    if (!b) return 0;
    n = boxes(src, b);
    ok = sweep(dst, op, b, n, m);
    m->free(m->ctx, b, size);
    return ok;
}
