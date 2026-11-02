/* SPDX-License-Identifier: MIT */
/* GFOpenGfx graphics.library 47.101: the drawing dispatch (gfx-dispatch, a
 * part of the face). Copyright (c) 2026 AmiCradle Team. MIT.
 *
 * A provider (GFOpenRTG, for its screens) and a look (GFOpenLook, for the
 * system's look) register here, and graphics.library's drawing calls ask
 * them before they draw, with no SetFunction. The records, the order and
 * the rules are GFOpenGfx's in gfopengpu.library (gfopenrtg
 * library/ogfx/ogfx_lib.c, OGFX interface 1.5; include/gfopengpu/gfx.h),
 * ported as they are:
 *
 * - RectFill and Text ask the look first, then the provider; the other 20
 *   of struct OGFXProviderAll's calls ask the provider. A v1 record (or a
 *   1.0 one, the first four calls) is copied as far as it goes and the rest
 *   is zero, so the calls it has no room for are never the provider's. A
 *   NULL entry, or an entry that answers 0, is "not mine".
 * - The request holds the arguments as graphics.library reads them (16-bit
 *   coordinates sign-extended, counts and lengths zero-extended, the pixel
 *   array calls' coordinates whole), exactly as ogfx_lib.c fills it.
 * - A call nobody takes goes to its owner's gfx_lvo_<Call> (OWNERS.md) with
 *   the registers as they came, so with nothing registered every call does
 *   what it did before the dispatch. AreaMove, AreaDraw, AreaEllipse,
 *   AreaEnd, DrawEllipse and EraseRect have no provider or look entry in
 *   1.5, so their vectors stay the owner's entries.
 * - The look may draw with RectFill and Text: they come back here and to the
 *   look again, which declines its own (gfx.h). look_busy counts the calls
 *   inside it; GFX_UnregisterLook works at once, and GFX_RegisterLook
 *   refuses while a call is still inside a look taken out.
 *
 * gfopengpu's own fast paths (the planar leaves, Chip RAM and AmiCradle
 * tests) are not here: graphics.library's own code is the core.
 *
 * Each gfx_dispatch_<Call> takes graphics.lvo's registers, as its
 * gfx_lvo_<Call> does (check_regs.py checks both), and gen_stubs.py puts it
 * in the vector table in the entry's place. Built by the Amiga compiler
 * with the NDK headers and include/graphics/gfxdispatch.h; elsewhere
 * GFX_REG is a plain parameter, for the tests. No .data, no .bss: the
 * state is gfx_private_of(GfxBase)->dispatch. */
#include <graphics/gfxdispatch.h>

#include "gfx_env.h"

#ifndef GFX_REG
#define GFX_REG(r, decl) decl
#endif

struct GfxBase;                            /* A6: only ever the library base */

struct gfx_dispatch {
    UBYTE have_provider;
    volatile UBYTE have_look;
    volatile UWORD look_busy;              /* calls inside the look now */
    struct OGFXProviderAll provider;       /* a v1 provider's calls are provider.v1's */
    struct OGFXLookV1 look;                /* kept when taken out, for calls still inside it */
};

GFX_STATIC_ASSERT(sizeof(struct gfx_dispatch) <= GFX_DISPATCH_BYTES, "the dispatch fits gfx_private");
#if defined(__m68k__) || defined(__mc68000__) || defined(mc68000)
GFX_STATIC_ASSERT(sizeof(struct OGFXProviderV1) == 48, "OGFXProviderV1 is gfx.h's");
GFX_STATIC_ASSERT(sizeof(struct OGFXProviderAll) == 104, "OGFXProviderAll is gfx.h's");
GFX_STATIC_ASSERT(sizeof(struct OGFXLookV1) == 24, "OGFXLookV1 is gfx.h's");
GFX_STATIC_ASSERT(OGFX_PROVIDER_V1_0_SIZE == 32, "a 1.0 record is four calls");
#endif

static inline struct gfx_dispatch *dispatch_of(struct GfxBase *gb)
{
    return (struct gfx_dispatch *)(void *)gfx_private_of(gb)->dispatch;
}

/* ---- the owners' entries (OWNERS.md), called when nobody takes a call ---- */

LONG gfx_lvo_Text(GFX_REG(a1, struct RastPort *rp), GFX_REG(a0, STRPTR text), GFX_REG(d0, ULONG length),
                  GFX_REG(a6, struct GfxBase *gb));
WORD gfx_lvo_TextLength(GFX_REG(a1, struct RastPort *rp), GFX_REG(a0, STRPTR text), GFX_REG(d0, ULONG length),
                        GFX_REG(a6, struct GfxBase *gb));
void gfx_lvo_TextExtent(GFX_REG(a1, struct RastPort *rp), GFX_REG(a0, STRPTR text), GFX_REG(d0, ULONG length),
                        GFX_REG(a2, struct TextExtent *extent), GFX_REG(a6, struct GfxBase *gb));
ULONG gfx_lvo_TextFit(GFX_REG(a1, struct RastPort *rp), GFX_REG(a0, STRPTR text), GFX_REG(d0, ULONG length),
                      GFX_REG(a2, struct TextExtent *extent), GFX_REG(a3, struct TextExtent *constraining),
                      GFX_REG(d1, LONG direction), GFX_REG(d2, ULONG bit_width), GFX_REG(d3, ULONG bit_height),
                      GFX_REG(a6, struct GfxBase *gb));
void gfx_lvo_RectFill(GFX_REG(a1, struct RastPort *rp), GFX_REG(d0, LONG x0), GFX_REG(d1, LONG y0),
                      GFX_REG(d2, LONG x1), GFX_REG(d3, LONG y1), GFX_REG(a6, struct GfxBase *gb));
LONG gfx_lvo_BltBitMap(GFX_REG(a0, struct BitMap *src), GFX_REG(d0, LONG sx), GFX_REG(d1, LONG sy),
                       GFX_REG(a1, struct BitMap *dst), GFX_REG(d2, LONG dx), GFX_REG(d3, LONG dy),
                       GFX_REG(d4, LONG width), GFX_REG(d5, LONG height), GFX_REG(d6, ULONG minterm),
                       GFX_REG(d7, ULONG mask), GFX_REG(a2, PLANEPTR temp), GFX_REG(a6, struct GfxBase *gb));
void gfx_lvo_BltTemplate(GFX_REG(a0, PLANEPTR source), GFX_REG(d0, LONG sx), GFX_REG(d1, LONG modulo),
                         GFX_REG(a1, struct RastPort *rp), GFX_REG(d2, LONG dx), GFX_REG(d3, LONG dy),
                         GFX_REG(d4, LONG width), GFX_REG(d5, LONG height), GFX_REG(a6, struct GfxBase *gb));
void gfx_lvo_ScrollRaster(GFX_REG(a1, struct RastPort *rp), GFX_REG(d0, LONG dx), GFX_REG(d1, LONG dy),
                          GFX_REG(d2, LONG x0), GFX_REG(d3, LONG y0), GFX_REG(d4, LONG x1), GFX_REG(d5, LONG y1),
                          GFX_REG(a6, struct GfxBase *gb));
void gfx_lvo_BltPattern(GFX_REG(a1, struct RastPort *rp), GFX_REG(a0, PLANEPTR mask), GFX_REG(d0, LONG x0),
                        GFX_REG(d1, LONG y0), GFX_REG(d2, LONG x1), GFX_REG(d3, LONG y1), GFX_REG(d4, LONG bpr),
                        GFX_REG(a6, struct GfxBase *gb));
void gfx_lvo_SetRast(GFX_REG(a1, struct RastPort *rp), GFX_REG(d0, ULONG pen), GFX_REG(a6, struct GfxBase *gb));
void gfx_lvo_Draw(GFX_REG(a1, struct RastPort *rp), GFX_REG(d0, LONG x), GFX_REG(d1, LONG y),
                  GFX_REG(a6, struct GfxBase *gb));
void gfx_lvo_PolyDraw(GFX_REG(a1, struct RastPort *rp), GFX_REG(d0, LONG count), GFX_REG(a0, WORD *xy),
                      GFX_REG(a6, struct GfxBase *gb));
LONG gfx_lvo_WritePixel(GFX_REG(a1, struct RastPort *rp), GFX_REG(d0, LONG x), GFX_REG(d1, LONG y),
                        GFX_REG(a6, struct GfxBase *gb));
LONG gfx_lvo_ReadPixel(GFX_REG(a1, struct RastPort *rp), GFX_REG(d0, LONG x), GFX_REG(d1, LONG y),
                       GFX_REG(a6, struct GfxBase *gb));
void gfx_lvo_BltBitMapRastPort(GFX_REG(a0, struct BitMap *src), GFX_REG(d0, LONG sx), GFX_REG(d1, LONG sy),
                               GFX_REG(a1, struct RastPort *rp), GFX_REG(d2, LONG dx), GFX_REG(d3, LONG dy),
                               GFX_REG(d4, LONG w), GFX_REG(d5, LONG h), GFX_REG(d6, ULONG minterm),
                               GFX_REG(a6, struct GfxBase *gb));
void gfx_lvo_BltMaskBitMapRastPort(GFX_REG(a0, struct BitMap *src), GFX_REG(d0, LONG sx), GFX_REG(d1, LONG sy),
                                   GFX_REG(a1, struct RastPort *rp), GFX_REG(d2, LONG dx), GFX_REG(d3, LONG dy),
                                   GFX_REG(d4, LONG w), GFX_REG(d5, LONG h), GFX_REG(d6, ULONG minterm),
                                   GFX_REG(a2, PLANEPTR mask), GFX_REG(a6, struct GfxBase *gb));
void gfx_lvo_ClipBlit(GFX_REG(a0, struct RastPort *srp), GFX_REG(d0, LONG sx), GFX_REG(d1, LONG sy),
                      GFX_REG(a1, struct RastPort *rp), GFX_REG(d2, LONG dx), GFX_REG(d3, LONG dy),
                      GFX_REG(d4, LONG w), GFX_REG(d5, LONG h), GFX_REG(d6, ULONG minterm),
                      GFX_REG(a6, struct GfxBase *gb));
void gfx_lvo_WriteChunkyPixels(GFX_REG(a0, struct RastPort *rp), GFX_REG(d0, ULONG x0), GFX_REG(d1, ULONG y0),
                               GFX_REG(d2, ULONG x1), GFX_REG(d3, ULONG y1), GFX_REG(a2, UBYTE *array),
                               GFX_REG(d4, LONG bpr), GFX_REG(a6, struct GfxBase *gb));
LONG gfx_lvo_WritePixelArray8(GFX_REG(a0, struct RastPort *rp), GFX_REG(d0, ULONG x0), GFX_REG(d1, ULONG y0),
                              GFX_REG(d2, ULONG x1), GFX_REG(d3, ULONG y1), GFX_REG(a2, UBYTE *array),
                              GFX_REG(a1, struct RastPort *tmp), GFX_REG(a6, struct GfxBase *gb));
LONG gfx_lvo_WritePixelLine8(GFX_REG(a0, struct RastPort *rp), GFX_REG(d0, ULONG x0), GFX_REG(d1, ULONG y0),
                             GFX_REG(d2, ULONG w), GFX_REG(a2, UBYTE *array), GFX_REG(a1, struct RastPort *tmp),
                             GFX_REG(a6, struct GfxBase *gb));
LONG gfx_lvo_ReadPixelLine8(GFX_REG(a0, struct RastPort *rp), GFX_REG(d0, ULONG x0), GFX_REG(d1, ULONG y0),
                            GFX_REG(d2, ULONG w), GFX_REG(a2, UBYTE *array), GFX_REG(a1, struct RastPort *tmp),
                            GFX_REG(a6, struct GfxBase *gb));
LONG gfx_lvo_ReadPixelArray8(GFX_REG(a0, struct RastPort *rp), GFX_REG(d0, ULONG x0), GFX_REG(d1, ULONG y0),
                             GFX_REG(d2, ULONG x1), GFX_REG(d3, ULONG y1), GFX_REG(a2, UBYTE *array),
                             GFX_REG(a1, struct RastPort *tmp), GFX_REG(a6, struct GfxBase *gb));

/* ---- the look (gfx.h): asked first on RectFill and Text --------------------
 * look_busy counts the calls inside the look, so its owner knows when its
 * code is free to go after GFX_UnregisterLook. A one-instruction add and
 * subtract on memory, which a task switch can't split. The count goes up
 * before the look is read: a caller that saw the look in is counted. */
static inline void look_enter(struct gfx_dispatch *d)
{
    __asm__ volatile ("addq.w #1,%0" : "+m" (d->look_busy) : : "cc");
}

static inline void look_leave(struct gfx_dispatch *d)
{
    __asm__ volatile ("subq.w #1,%0" : "+m" (d->look_busy) : : "cc");
}

static LONG look_rectfill(struct gfx_dispatch *d, struct RastPort *rp, LONG x0, LONG y0, LONG x1, LONG y1)
{
    LONG done = 0;
    LONG (*fn)(APTR, struct OGFXRectFillRequest *);
    look_enter(d);
    if (d->have_look && (fn = d->look.rectfill) != NULL) {
        struct OGFXRectFillRequest r;
        r.rp = rp; r.x0 = x0; r.y0 = y0; r.x1 = x1; r.y1 = y1;
        done = fn(d->look.userdata, &r);
    }
    look_leave(d);
    return done;
}

static LONG look_text(struct gfx_dispatch *d, struct RastPort *rp, STRPTR text, ULONG length, LONG *result)
{
    LONG done = 0;
    LONG (*fn)(APTR, struct OGFXTextRequest *);
    look_enter(d);
    if (d->have_look && (fn = d->look.text) != NULL) {
        struct OGFXTextRequest r;
        r.rp = rp; r.text = text; r.length = length; r.result = 0;
        if ((done = fn(d->look.userdata, &r)) != 0)
            *result = r.result;
    }
    look_leave(d);
    return done;
}

/* ---- asking: 1 when the look or the provider took the call --------------
 * Out of line, so a call with nothing registered pays two byte tests. */
#define ASK static __attribute__((noinline))
#define PROVIDES(call) (d->have_provider && d->provider.call)
#define UD (d->provider.v1.userdata)

ASK LONG ask_rectfill(struct gfx_dispatch *d, struct RastPort *rp, LONG x0, LONG y0, LONG x1, LONG y1)
{
    x0 = (WORD)x0; y0 = (WORD)y0; x1 = (WORD)x1; y1 = (WORD)y1;
    /* the look first: a window frame it draws is drawn whatever the bitmap */
    if (d->have_look && look_rectfill(d, rp, x0, y0, x1, y1))
        return 1;
    if (PROVIDES(v1.rectfill)) {
        struct OGFXRectFillRequest r;
        r.rp = rp; r.x0 = x0; r.y0 = y0; r.x1 = x1; r.y1 = y1;
        if (d->provider.v1.rectfill(UD, &r)) return 1;
    }
    return 0;
}

ASK LONG ask_text(struct gfx_dispatch *d, struct RastPort *rp, STRPTR text, ULONG length, LONG *result)
{
    length = (UWORD)length;
    if (d->have_look && look_text(d, rp, text, length, result))
        return 1;
    if (PROVIDES(v1.text)) {
        struct OGFXTextRequest r;
        r.rp = rp; r.text = text; r.length = length; r.result = 0;
        if (d->provider.v1.text(UD, &r)) { *result = r.result; return 1; }
    }
    return 0;
}

ASK LONG ask_bltbitmap(struct gfx_dispatch *d, struct BitMap *src, LONG sx, LONG sy, struct BitMap *dst,
                       LONG dx, LONG dy, LONG width, LONG height, ULONG minterm, ULONG mask, PLANEPTR temp,
                       LONG *result)
{
    struct OGFXBltBitMapRequest r;
    r.src = src; r.sx = (WORD)sx; r.sy = (WORD)sy;
    r.dst = dst; r.dx = (WORD)dx; r.dy = (WORD)dy;
    r.width = (WORD)width; r.height = (WORD)height;
    r.minterm = (UBYTE)minterm; r.mask = (UBYTE)mask; r.temp = temp; r.result = 0;
    if (d->provider.v1.bltbitmap(UD, &r)) { *result = r.result; return 1; }
    return 0;
}

ASK LONG ask_scroll(struct gfx_dispatch *d, struct RastPort *rp, LONG dx, LONG dy,
                    LONG x0, LONG y0, LONG x1, LONG y1)
{
    struct OGFXScrollRasterRequest r;
    r.rp = rp; r.dx = (WORD)dx; r.dy = (WORD)dy;
    r.x0 = (WORD)x0; r.y0 = (WORD)y0; r.x1 = (WORD)x1; r.y1 = (WORD)y1;
    return d->provider.v1.scrollraster(UD, &r) != 0;
}

ASK LONG ask_textlength(struct gfx_dispatch *d, struct RastPort *rp, STRPTR text, ULONG length, WORD *result)
{
    struct OGFXTextLengthRequest r;
    r.rp = rp; r.text = text; r.length = (UWORD)length; r.result = 0;
    if (d->provider.v1.textlength(UD, &r)) { *result = (WORD)r.result; return 1; }
    return 0;
}

ASK LONG ask_textextent(struct gfx_dispatch *d, struct RastPort *rp, STRPTR text, ULONG length,
                        struct TextExtent *extent)
{
    struct OGFXTextExtentRequest r;
    r.rp = rp; r.text = text; r.length = (UWORD)length; r.extent = extent;
    return d->provider.v1.textextent(UD, &r) != 0;
}

ASK LONG ask_textfit(struct gfx_dispatch *d, struct RastPort *rp, STRPTR text, ULONG length,
                     struct TextExtent *extent, struct TextExtent *constraining, LONG direction,
                     ULONG bit_width, ULONG bit_height, ULONG *result)
{
    struct OGFXTextFitRequest r;
    r.rp = rp; r.text = text; r.length = (UWORD)length; r.extent = extent;
    r.constraining = constraining; r.direction = (WORD)direction;
    r.bit_width = (UWORD)bit_width; r.bit_height = (UWORD)bit_height; r.result = 0;
    if (d->provider.v1.textfit(UD, &r)) { *result = r.result; return 1; }
    return 0;
}

ASK LONG ask_blttemplate(struct gfx_dispatch *d, PLANEPTR source, LONG sx, LONG modulo, struct RastPort *rp,
                         LONG dx, LONG dy, LONG width, LONG height)
{
    struct OGFXBltTemplateRequest r;
    r.source = source; r.sx = (WORD)sx; r.source_modulo = (WORD)modulo;
    r.rp = rp; r.dx = (WORD)dx; r.dy = (WORD)dy; r.width = (WORD)width; r.height = (WORD)height;
    return d->provider.v1.blttemplate(UD, &r) != 0;
}

ASK LONG ask_bltpattern(struct gfx_dispatch *d, struct RastPort *rp, PLANEPTR mask,
                        LONG x0, LONG y0, LONG x1, LONG y1, LONG bpr)
{
    struct OGFXBltPatternRequest r;
    r.rp = rp; r.mask = mask; r.x0 = (WORD)x0; r.y0 = (WORD)y0; r.x1 = (WORD)x1; r.y1 = (WORD)y1;
    r.mask_bpr = (WORD)bpr;
    return d->provider.bltpattern(UD, &r) != 0;
}

ASK LONG ask_setrast(struct gfx_dispatch *d, struct RastPort *rp, ULONG pen)
{
    struct OGFXSetRastRequest r;
    r.rp = rp; r.pen = (UBYTE)pen;
    return d->provider.setrast(UD, &r) != 0;
}

ASK LONG ask_draw(struct gfx_dispatch *d, struct RastPort *rp, LONG x, LONG y)
{
    struct OGFXDrawRequest r;
    r.rp = rp; r.x = (WORD)x; r.y = (WORD)y;
    return d->provider.draw(UD, &r) != 0;
}

ASK LONG ask_polydraw(struct gfx_dispatch *d, struct RastPort *rp, LONG n, WORD *xy)
{
    struct OGFXPolyDrawRequest r;
    r.rp = rp; r.count = (WORD)n; r.array = xy;
    return d->provider.polydraw(UD, &r) != 0;
}

ASK LONG ask_pixel(LONG (*fn)(APTR, struct OGFXPixelRequest *), APTR ud, struct RastPort *rp,
                   LONG x, LONG y, LONG *result)
{
    struct OGFXPixelRequest r;
    r.rp = rp; r.x = (WORD)x; r.y = (WORD)y; r.result = 0;
    if (fn(ud, &r)) { *result = r.result; return 1; }
    return 0;
}

ASK LONG ask_bltbmrp(LONG (*fn)(APTR, struct OGFXBltBitMapRastPortRequest *), APTR ud, struct BitMap *src,
                     LONG sx, LONG sy, struct RastPort *rp, LONG dx, LONG dy, LONG w, LONG h, ULONG minterm,
                     PLANEPTR mask)
{
    struct OGFXBltBitMapRastPortRequest r;
    r.src = src; r.sx = (WORD)sx; r.sy = (WORD)sy; r.rp = rp; r.dx = (WORD)dx; r.dy = (WORD)dy;
    r.width = (WORD)w; r.height = (WORD)h; r.minterm = (UBYTE)minterm; r.mask = mask;
    return fn(ud, &r) != 0;
}

ASK LONG ask_clipblit(struct gfx_dispatch *d, struct RastPort *srp, LONG sx, LONG sy, struct RastPort *rp,
                      LONG dx, LONG dy, LONG w, LONG h, ULONG minterm)
{
    struct OGFXClipBlitRequest r;
    r.src_rp = srp; r.sx = (WORD)sx; r.sy = (WORD)sy; r.rp = rp; r.dx = (WORD)dx; r.dy = (WORD)dy;
    r.width = (WORD)w; r.height = (WORD)h; r.minterm = (UBYTE)minterm;
    return d->provider.clipblit(UD, &r) != 0;
}

/* The pixel-array calls. line: WritePixelLine8 and ReadPixelLine8, whose
 * third register (x1 here) is the width. bpr is WriteChunkyPixels' only. */
ASK LONG ask_array(LONG (*fn)(APTR, struct OGFXPixelArrayRequest *), APTR ud, int line, struct RastPort *rp,
                   ULONG x0, ULONG y0, ULONG x1, ULONG y1, UBYTE *array, LONG bpr, struct RastPort *tmp,
                   LONG *result)
{
    struct OGFXPixelArrayRequest r;
    r.rp = rp; r.array = array; r.bytes_per_row = bpr; r.temp_rp = tmp; r.result = 0;
    if (line) {
        r.width = (UWORD)x1;
        r.x0 = (UWORD)x0; r.y0 = (UWORD)y0; r.x1 = r.x0 + (LONG)r.width - 1; r.y1 = r.y0;
    } else {
        r.width = 0;
        r.x0 = (UWORD)x0; r.y0 = (UWORD)y0; r.x1 = (UWORD)x1; r.y1 = (UWORD)y1;
    }
    if (fn(ud, &r)) { *result = r.result; return 1; }
    return 0;
}

/* ---- the entries, in graphics.lvo's registers ------------------------------ */

void gfx_dispatch_RectFill(GFX_REG(a1, struct RastPort *rp), GFX_REG(d0, LONG x0), GFX_REG(d1, LONG y0),
                           GFX_REG(d2, LONG x1), GFX_REG(d3, LONG y1), GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    if ((d->have_look | d->have_provider) && ask_rectfill(d, rp, x0, y0, x1, y1))
        return;
    gfx_lvo_RectFill(rp, x0, y0, x1, y1, gb);
}

LONG gfx_dispatch_Text(GFX_REG(a1, struct RastPort *rp), GFX_REG(a0, STRPTR text), GFX_REG(d0, ULONG length),
                       GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    LONG result;
    if ((d->have_look | d->have_provider) && ask_text(d, rp, text, length, &result))
        return result;
    return gfx_lvo_Text(rp, text, length, gb);
}

LONG gfx_dispatch_BltBitMap(GFX_REG(a0, struct BitMap *src), GFX_REG(d0, LONG sx), GFX_REG(d1, LONG sy),
                            GFX_REG(a1, struct BitMap *dst), GFX_REG(d2, LONG dx), GFX_REG(d3, LONG dy),
                            GFX_REG(d4, LONG width), GFX_REG(d5, LONG height), GFX_REG(d6, ULONG minterm),
                            GFX_REG(d7, ULONG mask), GFX_REG(a2, PLANEPTR temp), GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    LONG result;
    if (PROVIDES(v1.bltbitmap) &&
        ask_bltbitmap(d, src, sx, sy, dst, dx, dy, width, height, minterm, mask, temp, &result))
        return result;
    return gfx_lvo_BltBitMap(src, sx, sy, dst, dx, dy, width, height, minterm, mask, temp, gb);
}

void gfx_dispatch_ScrollRaster(GFX_REG(a1, struct RastPort *rp), GFX_REG(d0, LONG dx), GFX_REG(d1, LONG dy),
                               GFX_REG(d2, LONG x0), GFX_REG(d3, LONG y0), GFX_REG(d4, LONG x1), GFX_REG(d5, LONG y1),
                               GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    if (PROVIDES(v1.scrollraster) && ask_scroll(d, rp, dx, dy, x0, y0, x1, y1))
        return;
    gfx_lvo_ScrollRaster(rp, dx, dy, x0, y0, x1, y1, gb);
}

WORD gfx_dispatch_TextLength(GFX_REG(a1, struct RastPort *rp), GFX_REG(a0, STRPTR text), GFX_REG(d0, ULONG length),
                             GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    WORD result;
    if (PROVIDES(v1.textlength) && ask_textlength(d, rp, text, length, &result))
        return result;
    return gfx_lvo_TextLength(rp, text, length, gb);
}

void gfx_dispatch_TextExtent(GFX_REG(a1, struct RastPort *rp), GFX_REG(a0, STRPTR text), GFX_REG(d0, ULONG length),
                             GFX_REG(a2, struct TextExtent *extent), GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    if (PROVIDES(v1.textextent) && ask_textextent(d, rp, text, length, extent))
        return;
    gfx_lvo_TextExtent(rp, text, length, extent, gb);
}

ULONG gfx_dispatch_TextFit(GFX_REG(a1, struct RastPort *rp), GFX_REG(a0, STRPTR text), GFX_REG(d0, ULONG length),
                           GFX_REG(a2, struct TextExtent *extent), GFX_REG(a3, struct TextExtent *constraining),
                           GFX_REG(d1, LONG direction), GFX_REG(d2, ULONG bit_width), GFX_REG(d3, ULONG bit_height),
                           GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    ULONG result;
    if (PROVIDES(v1.textfit) &&
        ask_textfit(d, rp, text, length, extent, constraining, direction, bit_width, bit_height, &result))
        return result;
    return gfx_lvo_TextFit(rp, text, length, extent, constraining, direction, bit_width, bit_height, gb);
}

void gfx_dispatch_BltTemplate(GFX_REG(a0, PLANEPTR source), GFX_REG(d0, LONG sx), GFX_REG(d1, LONG modulo),
                              GFX_REG(a1, struct RastPort *rp), GFX_REG(d2, LONG dx), GFX_REG(d3, LONG dy),
                              GFX_REG(d4, LONG width), GFX_REG(d5, LONG height), GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    if (PROVIDES(v1.blttemplate) && ask_blttemplate(d, source, sx, modulo, rp, dx, dy, width, height))
        return;
    gfx_lvo_BltTemplate(source, sx, modulo, rp, dx, dy, width, height, gb);
}

void gfx_dispatch_BltPattern(GFX_REG(a1, struct RastPort *rp), GFX_REG(a0, PLANEPTR mask), GFX_REG(d0, LONG x0),
                             GFX_REG(d1, LONG y0), GFX_REG(d2, LONG x1), GFX_REG(d3, LONG y1), GFX_REG(d4, LONG bpr),
                             GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    if (PROVIDES(bltpattern) && ask_bltpattern(d, rp, mask, x0, y0, x1, y1, bpr))
        return;
    gfx_lvo_BltPattern(rp, mask, x0, y0, x1, y1, bpr, gb);
}

void gfx_dispatch_SetRast(GFX_REG(a1, struct RastPort *rp), GFX_REG(d0, ULONG pen), GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    if (PROVIDES(setrast) && ask_setrast(d, rp, pen))
        return;
    gfx_lvo_SetRast(rp, pen, gb);
}

void gfx_dispatch_Draw(GFX_REG(a1, struct RastPort *rp), GFX_REG(d0, LONG x), GFX_REG(d1, LONG y),
                       GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    if (PROVIDES(draw) && ask_draw(d, rp, x, y))
        return;
    gfx_lvo_Draw(rp, x, y, gb);
}

void gfx_dispatch_PolyDraw(GFX_REG(a1, struct RastPort *rp), GFX_REG(d0, LONG count), GFX_REG(a0, WORD *xy),
                           GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    if (PROVIDES(polydraw) && ask_polydraw(d, rp, count, xy))
        return;
    gfx_lvo_PolyDraw(rp, count, xy, gb);
}

LONG gfx_dispatch_WritePixel(GFX_REG(a1, struct RastPort *rp), GFX_REG(d0, LONG x), GFX_REG(d1, LONG y),
                             GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    LONG result;
    if (PROVIDES(writepixel) && ask_pixel(d->provider.writepixel, UD, rp, x, y, &result))
        return result;
    return gfx_lvo_WritePixel(rp, x, y, gb);
}

LONG gfx_dispatch_ReadPixel(GFX_REG(a1, struct RastPort *rp), GFX_REG(d0, LONG x), GFX_REG(d1, LONG y),
                            GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    LONG result;
    if (PROVIDES(readpixel) && ask_pixel(d->provider.readpixel, UD, rp, x, y, &result))
        return result;
    return gfx_lvo_ReadPixel(rp, x, y, gb);
}

void gfx_dispatch_BltBitMapRastPort(GFX_REG(a0, struct BitMap *src), GFX_REG(d0, LONG sx), GFX_REG(d1, LONG sy),
                                    GFX_REG(a1, struct RastPort *rp), GFX_REG(d2, LONG dx), GFX_REG(d3, LONG dy),
                                    GFX_REG(d4, LONG w), GFX_REG(d5, LONG h), GFX_REG(d6, ULONG minterm),
                                    GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    if (PROVIDES(bltbitmaprastport) &&
        ask_bltbmrp(d->provider.bltbitmaprastport, UD, src, sx, sy, rp, dx, dy, w, h, minterm, NULL))
        return;
    gfx_lvo_BltBitMapRastPort(src, sx, sy, rp, dx, dy, w, h, minterm, gb);
}

void gfx_dispatch_BltMaskBitMapRastPort(GFX_REG(a0, struct BitMap *src), GFX_REG(d0, LONG sx), GFX_REG(d1, LONG sy),
                                        GFX_REG(a1, struct RastPort *rp), GFX_REG(d2, LONG dx), GFX_REG(d3, LONG dy),
                                        GFX_REG(d4, LONG w), GFX_REG(d5, LONG h), GFX_REG(d6, ULONG minterm),
                                        GFX_REG(a2, PLANEPTR mask), GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    if (PROVIDES(bltmaskbitmaprastport) &&
        ask_bltbmrp(d->provider.bltmaskbitmaprastport, UD, src, sx, sy, rp, dx, dy, w, h, minterm, mask))
        return;
    gfx_lvo_BltMaskBitMapRastPort(src, sx, sy, rp, dx, dy, w, h, minterm, mask, gb);
}

void gfx_dispatch_ClipBlit(GFX_REG(a0, struct RastPort *srp), GFX_REG(d0, LONG sx), GFX_REG(d1, LONG sy),
                           GFX_REG(a1, struct RastPort *rp), GFX_REG(d2, LONG dx), GFX_REG(d3, LONG dy),
                           GFX_REG(d4, LONG w), GFX_REG(d5, LONG h), GFX_REG(d6, ULONG minterm),
                           GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    if (PROVIDES(clipblit) && ask_clipblit(d, srp, sx, sy, rp, dx, dy, w, h, minterm))
        return;
    gfx_lvo_ClipBlit(srp, sx, sy, rp, dx, dy, w, h, minterm, gb);
}

void gfx_dispatch_WriteChunkyPixels(GFX_REG(a0, struct RastPort *rp), GFX_REG(d0, ULONG x0), GFX_REG(d1, ULONG y0),
                                    GFX_REG(d2, ULONG x1), GFX_REG(d3, ULONG y1), GFX_REG(a2, UBYTE *array),
                                    GFX_REG(d4, LONG bpr), GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    LONG result;
    if (PROVIDES(writechunkypixels) &&
        ask_array(d->provider.writechunkypixels, UD, 0, rp, x0, y0, x1, y1, array, bpr, NULL, &result))
        return;
    gfx_lvo_WriteChunkyPixels(rp, x0, y0, x1, y1, array, bpr, gb);
}

LONG gfx_dispatch_WritePixelArray8(GFX_REG(a0, struct RastPort *rp), GFX_REG(d0, ULONG x0), GFX_REG(d1, ULONG y0),
                                   GFX_REG(d2, ULONG x1), GFX_REG(d3, ULONG y1), GFX_REG(a2, UBYTE *array),
                                   GFX_REG(a1, struct RastPort *tmp), GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    LONG result;
    if (PROVIDES(writepixelarray8) &&
        ask_array(d->provider.writepixelarray8, UD, 0, rp, x0, y0, x1, y1, array, 0, tmp, &result))
        return result;
    return gfx_lvo_WritePixelArray8(rp, x0, y0, x1, y1, array, tmp, gb);
}

LONG gfx_dispatch_WritePixelLine8(GFX_REG(a0, struct RastPort *rp), GFX_REG(d0, ULONG x0), GFX_REG(d1, ULONG y0),
                                  GFX_REG(d2, ULONG w), GFX_REG(a2, UBYTE *array), GFX_REG(a1, struct RastPort *tmp),
                                  GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    LONG result;
    if (PROVIDES(writepixelline8) &&
        ask_array(d->provider.writepixelline8, UD, 1, rp, x0, y0, w, 0, array, 0, tmp, &result))
        return result;
    return gfx_lvo_WritePixelLine8(rp, x0, y0, w, array, tmp, gb);
}

LONG gfx_dispatch_ReadPixelLine8(GFX_REG(a0, struct RastPort *rp), GFX_REG(d0, ULONG x0), GFX_REG(d1, ULONG y0),
                                 GFX_REG(d2, ULONG w), GFX_REG(a2, UBYTE *array), GFX_REG(a1, struct RastPort *tmp),
                                 GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    LONG result;
    if (PROVIDES(readpixelline8) &&
        ask_array(d->provider.readpixelline8, UD, 1, rp, x0, y0, w, 0, array, 0, tmp, &result))
        return result;
    return gfx_lvo_ReadPixelLine8(rp, x0, y0, w, array, tmp, gb);
}

LONG gfx_dispatch_ReadPixelArray8(GFX_REG(a0, struct RastPort *rp), GFX_REG(d0, ULONG x0), GFX_REG(d1, ULONG y0),
                                  GFX_REG(d2, ULONG x1), GFX_REG(d3, ULONG y1), GFX_REG(a2, UBYTE *array),
                                  GFX_REG(a1, struct RastPort *tmp), GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    LONG result;
    if (PROVIDES(readpixelarray8) &&
        ask_array(d->provider.readpixelarray8, UD, 0, rp, x0, y0, x1, y1, array, 0, tmp, &result))
        return result;
    return gfx_lvo_ReadPixelArray8(rp, x0, y0, x1, y1, array, tmp, gb);
}

/* ---- the registry: graphics.library 47.101's own LVOs (-1062 to -1092) ------
 * gfopengpu.library's OGFX_Version, OGFX_RegisterProvider,
 * OGFX_UnregisterProvider, OGFX_RegisterLook, OGFX_UnregisterLook and
 * OGFX_Status, with the same arguments, registers and answers. */

static void forbid(struct GfxBase *gb) { struct gfx_env *e = gfx_env_of(gb); e->forbid(e->ctx); }
static void permit(struct GfxBase *gb) { struct gfx_env *e = gfx_env_of(gb); e->permit(e->ctx); }

ULONG gfx_lvo_GFX_DispatchVersion(GFX_REG(a6, struct GfxBase *gb))
{
    (void)gb;
    return ((ULONG)GFXDISPATCH_INTERFACE_VERSION << 16) | GFXDISPATCH_INTERFACE_REVISION;
}

LONG gfx_lvo_GFX_RegisterProvider(GFX_REG(a0, struct OGFXProviderV1 *provider), GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    ULONG size, i;
    if (!provider || provider->abi != OGFX_PROVIDER_ABI_V1 ||
        provider->size < OGFX_PROVIDER_V1_0_SIZE || !provider->owner)
        return 0;
    /* a v1 provider's record, or a 1.4 one's (struct OGFXProviderAll) */
    size = provider->size < sizeof(struct OGFXProviderAll) ? provider->size : sizeof(struct OGFXProviderAll);

    forbid(gb);
    {   /* a 1.0 or v1 provider's record is shorter: what it has no room for stays NULL */
        volatile UBYTE *to = (volatile UBYTE *)&d->provider;
        const UBYTE *from = (const UBYTE *)provider;
        for (i = 0; i < sizeof(d->provider); ++i)
            to[i] = i < size ? from[i] : 0;
    }
    d->have_provider = 1;
    permit(gb);
    return 1;
}

LONG gfx_lvo_GFX_UnregisterProvider(GFX_REG(a0, APTR owner), GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    LONG removed = 0;
    forbid(gb);
    if (d->have_provider && owner && d->provider.v1.owner == owner) {
        volatile UBYTE *p = (volatile UBYTE *)&d->provider;
        ULONG i;
        for (i = 0; i < sizeof(d->provider); ++i) p[i] = 0;
        d->have_provider = 0;
        removed = 1;
    }
    permit(gb);
    return removed;
}

/* Taking the look out leaves the record as it was, so a call already
 * inside it still has its function and userdata; a new look waits until
 * those calls are done. */
LONG gfx_lvo_GFX_RegisterLook(GFX_REG(a0, struct OGFXLookV1 *look), GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    LONG ok = 0;
    if (!look || look->abi != OGFX_LOOK_ABI_V1 || look->size < sizeof(struct OGFXLookV1) || !look->owner)
        return 0;
    forbid(gb);
    if (!d->have_look && !d->look_busy) {
        d->look.size = sizeof(struct OGFXLookV1);
        d->look.abi = OGFX_LOOK_ABI_V1;
        d->look.owner = look->owner;
        d->look.userdata = look->userdata;
        d->look.rectfill = look->rectfill;
        d->look.text = look->text;
        d->have_look = 1;
        ok = 1;
    }
    permit(gb);
    return ok;
}

LONG gfx_lvo_GFX_UnregisterLook(GFX_REG(a0, APTR owner), GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    LONG removed = 0;
    forbid(gb);
    if (d->have_look && owner && d->look.owner == owner) {
        d->have_look = 0;
        removed = 1;
    }
    permit(gb);
    return removed;
}

ULONG gfx_lvo_GFX_DispatchStatus(GFX_REG(a6, struct GfxBase *gb))
{
    struct gfx_dispatch *d = dispatch_of(gb);
    /* every call goes through the dispatch, and graphics.library's own
     * drawing is always on: PATCHED and ENABLED, as gfopengpu answers once
     * its patches are in and it is enabled */
    ULONG status = OGFX_STATUS_PATCHED | OGFX_STATUS_ENABLED;
    if (d->have_provider) status |= OGFX_STATUS_PROVIDER;
    if (d->have_look) status |= OGFX_STATUS_LOOK;
    if (d->look_busy) status |= OGFX_STATUS_LOOK_BUSY;
    return status;
}

/* lib_init, before any area's init: nothing registered */
void gfx_clear_dispatch(struct gfx_private *pv)
{
    volatile uint32_t *p = pv->dispatch;
    uint32_t i;
    for (i = 0; i < GFX_DISPATCH_BYTES / 4; i++) p[i] = 0;
}
