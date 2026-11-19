/* SPDX-License-Identifier: MIT */
/* gfopenoverlay.library 1.0: Overlay's calls (libraries/gfopenoverlay.h; the design is gfosdesign
 * gfos/libraries/gfopenlayout-overlay, v2, agreed by Dale on 19 Nov 2026: "v2 is where they live").
 *
 * layers.library keeps what only it can know: where the box is on the screen, what of it shows,
 * the hole a composited screen leaves for it, and the place record it writes after every change of
 * the stack (GFOpenLayout's layers_comp.c). Its four overlay entries (-264 to -282, 47.111) are the
 * private hook this library calls; programs call this library, with a window, and never layers'.
 * The box is given in the window's inner area here; layers has it in the layer (the whole window).
 *
 * Copyright (c) 2026 AmiCradle Team. MIT. */
#include <exec/types.h>
#include <exec/resident.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <utility/tagitem.h>
#include <proto/exec.h>
#include <libraries/gfopenoverlay.h>

#define REG(r, decl) register decl __asm(#r)

#define LIB_VERSION  1
#define LIB_REVISION 0
#define LAYERS_NEEDS 111            /* layers.library 47.111: the overlay entries */

struct GOBase {
    struct Library lib;
    BPTR seglist;
    struct ExecBase *sys;
    struct Library *layers;
};

/* What a caller holds: layers' overlay and the window, for the inner area on SetOverlayAttrs */
struct GOverlay {
    APTR lov;
    struct Window *win;
};

int start(void) { return -1; }

static const char lib_name[] = GFOPENOVERLAY_NAME;
static const char lib_id[] = "gfopenoverlay.library 1.0 (19.11.2026) AmiCradle Team\r\n";
static const char lib_ver[] __attribute__((used)) = "\0$VER: gfopenoverlay.library 1.0 (19.11.2026) AmiCradle Team";
static const char layers_name[] = "layers.library";

#define SysBase (base->sys)

/* ---- layers.library's private hook ----------------------------------------------------------- */

static ULONG lcall(struct Library *lb, LONG lvo, ULONG a0v, ULONG a1v)
{
    register ULONG d0 __asm("d0");
    register ULONG a0 __asm("a0") = a0v;
    register ULONG a1 __asm("a1") = a1v;
    register struct Library *a6 __asm("a6") = lb;
    register ULONG d1 __asm("d1");
    __asm volatile("jsr (%5)"
                   : "=r"(d0), "=r"(d1), "+r"(a0), "+r"(a1)
                   : "r"(a6), "a"((char *)lb + lvo)
                   : "cc", "memory");
    (void)d1;
    return d0;
}

/* ---- tags ------------------------------------------------------------------------------------ */

static const struct TagItem *tag_find(const struct TagItem *t, ULONG tag)
{
    while (t) {
        switch (t->ti_Tag) {
        case TAG_DONE: return 0;
        case TAG_IGNORE: t++; continue;
        case TAG_MORE: t = (const struct TagItem *)t->ti_Data; continue;
        case TAG_SKIP: t += 1 + t->ti_Data; continue;
        }
        if (t->ti_Tag == tag) return t;
        t++;
    }
    return 0;
}

/* The tags layers gets: the box moved from the window's inner area to the layer (the whole
 * window), the rest as given. Returns 0 if the box is empty. */
static int to_layer(struct Window *w, const struct TagItem *in, struct TagItem *out, int keep_box)
{
    LONG iw = w->Width - w->BorderLeft - w->BorderRight, ih = w->Height - w->BorderTop - w->BorderBottom;
    const struct TagItem *t;
    LONG x = (t = tag_find(in, OVA_Left)) ? (LONG)t->ti_Data : 0;
    LONG y = (t = tag_find(in, OVA_Top)) ? (LONG)t->ti_Data : 0;
    LONG bw = (t = tag_find(in, OVA_Width)) ? (LONG)t->ti_Data : iw - x;
    LONG bh = (t = tag_find(in, OVA_Height)) ? (LONG)t->ti_Data : ih - y;
    int n = 0;
    static const ULONG pass[] = { OVA_PicWidth, OVA_PicHeight, OVA_Aspect };
    unsigned k;
    if (bw < 1 || bh < 1) return 0;
    if (!keep_box || tag_find(in, OVA_Left) || tag_find(in, OVA_Top) || tag_find(in, OVA_Width) || tag_find(in, OVA_Height)
        || !(tag_find(in, OVA_PicWidth) || tag_find(in, OVA_PicHeight) || tag_find(in, OVA_Aspect))) {
        out[n].ti_Tag = OVA_Left;   out[n++].ti_Data = (ULONG)(w->BorderLeft + x);
        out[n].ti_Tag = OVA_Top;    out[n++].ti_Data = (ULONG)(w->BorderTop + y);
        out[n].ti_Tag = OVA_Width;  out[n++].ti_Data = (ULONG)bw;
        out[n].ti_Tag = OVA_Height; out[n++].ti_Data = (ULONG)bh;
    }
    for (k = 0; k < sizeof pass / sizeof pass[0]; k++)
        if ((t = tag_find(in, pass[k])) != 0) { out[n].ti_Tag = t->ti_Tag; out[n++].ti_Data = t->ti_Data; }
    out[n].ti_Tag = TAG_DONE;
    out[n].ti_Data = 0;
    return 1;
}

/* ---- the calls ------------------------------------------------------------------------------- */

static APTR L_OpenOverlay(REG(a0, struct Window *w), REG(a1, const struct TagItem *tags), REG(a6, struct GOBase *base))
{
    struct TagItem lt[8];
    struct GOverlay *ov;
    if (!w || !w->WLayer || !to_layer(w, tags, lt, 0)) return 0;
    if (!(ov = AllocMem(sizeof *ov, MEMF_PUBLIC | MEMF_CLEAR))) return 0;
    ov->win = w;
    ov->lov = (APTR)lcall(base->layers, -264, (ULONG)w->WLayer, (ULONG)lt);
    if (!ov->lov) { FreeMem(ov, sizeof *ov); return 0; }
    return ov;
}

static struct LayerOverlayPlace *L_OverlayPlace(REG(a0, struct GOverlay *ov), REG(a6, struct GOBase *base))
{
    return ov ? (struct LayerOverlayPlace *)lcall(base->layers, -270, (ULONG)ov->lov, 0) : 0;
}

/* With no tags at all the box goes back to the whole inner area: what a program calls after its
 * window was sized. With only OVA_PicWidth, OVA_PicHeight or OVA_Aspect, only those change. */
static LONG L_SetOverlayAttrs(REG(a0, struct GOverlay *ov), REG(a1, const struct TagItem *tags), REG(a6, struct GOBase *base))
{
    struct TagItem lt[8];
    if (!ov || !to_layer(ov->win, tags, lt, 1)) return 0;
    return (LONG)lcall(base->layers, -276, (ULONG)ov->lov, (ULONG)lt);
}

static void L_CloseOverlay(REG(a0, struct GOverlay *ov), REG(a6, struct GOBase *base))
{
    if (!ov) return;
    lcall(base->layers, -282, (ULONG)ov->lov, 0);
    FreeMem(ov, sizeof *ov);
}

/* ---- the library ----------------------------------------------------------------------------- */

static struct Library *lib_init(REG(d0, struct GOBase *base), REG(a0, BPTR seglist), REG(a6, struct ExecBase *sys))
{
    base->sys = sys;
    base->seglist = seglist;
    base->lib.lib_Node.ln_Type = NT_LIBRARY;
    base->lib.lib_Node.ln_Name = (char *)lib_name;
    base->lib.lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
    base->lib.lib_Version = LIB_VERSION;
    base->lib.lib_Revision = LIB_REVISION;
    base->lib.lib_IdString = (APTR)lib_id;
    return &base->lib;
}

/* Opens only over a layers.library with the overlay entries (47.111 and up) */
static struct Library *lib_open(REG(a6, struct GOBase *base))
{
    if (!base->layers) {
        struct Library *l = OpenLibrary((CONST_STRPTR)layers_name, 47);
        if (!l) return 0;
        if (l->lib_Version == 47 && l->lib_Revision < LAYERS_NEEDS) { CloseLibrary(l); return 0; }
        base->layers = l;
    }
    base->lib.lib_OpenCnt++;
    base->lib.lib_Flags &= ~LIBF_DELEXP;
    return &base->lib;
}

static BPTR lib_expunge(REG(a6, struct GOBase *base))
{
    BPTR seglist;
    if (base->lib.lib_OpenCnt) {
        base->lib.lib_Flags |= LIBF_DELEXP;
        return 0;
    }
    seglist = base->seglist;
    Remove(&base->lib.lib_Node);
    if (base->layers) CloseLibrary(base->layers);
    FreeMem((UBYTE *)base - base->lib.lib_NegSize, base->lib.lib_NegSize + base->lib.lib_PosSize);
    return seglist;
}

static BPTR lib_close(REG(a6, struct GOBase *base))
{
    if (base->lib.lib_OpenCnt) base->lib.lib_OpenCnt--;
    if (base->lib.lib_OpenCnt == 0 && (base->lib.lib_Flags & LIBF_DELEXP)) return lib_expunge(base);
    return 0;
}

static ULONG lib_null(void) { return 0; }

static const APTR lib_vectors[] = {
    (APTR)lib_open, (APTR)lib_close, (APTR)lib_expunge, (APTR)lib_null,
    (APTR)L_OpenOverlay,        /* -30 */
    (APTR)L_OverlayPlace,       /* -36 */
    (APTR)L_SetOverlayAttrs,    /* -42 */
    (APTR)L_CloseOverlay,       /* -48 */
    (APTR)-1
};

static const struct {
    ULONG size;
    const APTR *vectors;
    APTR data;
    APTR init;
} lib_inittable = { sizeof(struct GOBase), lib_vectors, NULL, (APTR)lib_init };

const struct Resident lib_romtag = {
    RTC_MATCHWORD, (struct Resident *)&lib_romtag, (APTR)(&lib_romtag + 1),
    RTF_AUTOINIT, LIB_VERSION, NT_LIBRARY, 0,
    (char *)lib_name, (char *)lib_id, (APTR)&lib_inittable
};
