/* SPDX-License-Identifier: MIT */
/* GFOpenGfx graphics.library 47.101 and up: drawing dispatch, the provider
 * and look registry. Copyright (c) 2026 AmiCradle Team. MIT.
 *
 * GF extension, graphics.library 47.101 (not OS 3.2's): six calls after OS
 * 3.2's last LVO (-1056 WriteChunkyPixels), with the arguments, registers
 * and results of gfopengpu.library's OGFX_ calls of the same job, and from
 * 47.102 a seventh, GFX_RegisterProviderPri:
 *
 *  -1062 ULONG GFX_DispatchVersion()()                          OGFX_Version
 *  -1068 LONG  GFX_RegisterProvider(struct OGFXProviderV1 *)(a0) OGFX_RegisterProvider
 *  -1074 LONG  GFX_UnregisterProvider(APTR owner)(a0)            OGFX_UnregisterProvider
 *  -1080 LONG  GFX_RegisterLook(struct OGFXLookV1 *)(a0)         OGFX_RegisterLook
 *  -1086 LONG  GFX_UnregisterLook(APTR owner)(a0)                OGFX_UnregisterLook
 *  -1092 ULONG GFX_DispatchStatus()()                           OGFX_Status
 *  -1098 LONG  GFX_RegisterProviderPri(struct OGFXProviderV1 *, LONG pri)(a0, d0)  (47.102)
 *
 * graphics.library's own drawing calls ask the look (RectFill and Text),
 * then the providers (the 22 calls of struct OGFXProviderAll), then draw
 * themselves, with no SetFunction: the rules are those of GFOpenGfx in
 * gfopengpu.library (include/gfopengpu/gfx.h, OGFX interface 1.5), which
 * the records, requests and status bits below repeat word for word, under
 * the same names and guards, so a provider builds against either header,
 * or both in either order. GFX_DispatchVersion() answers 1.5 in 47.101, as
 * OGFX_Version() does, and 1.6 from 47.102 (1.5 and the provider chain).
 * GFX_DispatchStatus() always has OGFX_STATUS_PATCHED and
 * OGFX_STATUS_ENABLED (every call of graphics.library goes through the
 * dispatch, and its own drawing is on), never OGFX_STATUS_AMICRADLE; LOOK
 * and LOOK_BUSY as in gfx.h, PROVIDER while one provider or more is in.
 *
 * The provider chain (graphics.library 47.102, new; gfx.h's text below says
 * "one provider", as gfopengpu.library and graphics.library 47.101 keep):
 *
 *   - Up to GFXDISPATCH_PROVIDERS providers are in at once, each with its
 *     own copied record (gfx.h's rules for each: the record copied as far as
 *     its size goes, the rest NULL; its requests filled the same way).
 *   - They are asked in order of priority, highest first; equals in the
 *     order they came in. A call asks the look (RectFill and Text), then each
 *     provider whose record has the call, until one answers non-zero, then
 *     graphics.library's own code. Each provider gets the request as it was
 *     filled, whatever one before it wrote there.
 *   - GFX_RegisterProviderPri(record, pri) registers at pri (the low byte,
 *     signed, as ln_Pri); GFX_RegisterProvider(record) is
 *     GFX_RegisterProviderPri(record, GFXDISPATCH_PRI_DEFAULT). The
 *     priority is an argument, not a field: the records stay gfx.h 1.5's.
 *   - An owner already in that registers again replaces its own entry (the
 *     new record and priority; at the same priority it keeps its place),
 *     never another's. A new owner when every slot is taken is refused (0).
 *   - GFX_UnregisterProvider(owner) takes out that owner's provider only;
 *     calls from then on don't reach it, a call already inside it runs on
 *     (its slot keeps the record until a later registration reuses it).
 *   - The priorities in use: GFXDISPATCH_PRI_RTG for an RTG system's
 *     provider, which draws on its own bitmaps and declines the rest
 *     (GFOpenRTG); GFXDISPATCH_PRI_DEFAULT for GFX_RegisterProvider and
 *     for a provider registered through gfopengpu.library's
 *     OGFX_RegisterProvider; GFXDISPATCH_PRI_FAST for gfopengpu.library's
 *     planar fast paths, asked for what the others decline.
 *   - Old providers need no change: on 47.101 GFX_RegisterProvider still
 *     takes the one place, on 47.102 it joins the chain at priority 0.
 *
 * Ask first: GFX_DISPATCH_AVAILABLE(GfxBase) says the calls are there (a
 * jump table that reaches -1092, and graphics.library 47.101 or later);
 * GFX_DISPATCH_CHAIN_AVAILABLE(GfxBase) says the chain and
 * GFX_RegisterProviderPri are (-1098, and 47.102 or later). */
#ifndef GRAPHICS_GFXDISPATCH_H
#define GRAPHICS_GFXDISPATCH_H

/* ---- include/gfopengpu/gfx.h (DalsinAI/gfopenrtg), OGFX interface 1.5:
 * from here to its end, word for word. Keep in step with it. ---- */
#ifndef OPENGPU_GFX_H
#define OPENGPU_GFX_H

#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/text.h>

#define OPENGPU_OGFX_REVISION 6          /* gfopengpu.library 0.6: GFOpenGfx inside */
#define OPENGPU_OGFX_LOOK_REVISION 7     /* gfopengpu.library 0.7: the look hook */
#define OPENGPU_OGFX_ALL_REVISION 8      /* gfopengpu.library 0.8: every drawing call */

/* OGFX_Version(): (version << 16) | revision of the GFOpenGfx interface.
 * 1.2 is gfopengfx.library 1.1's interface inside gfopengpu.library, with the
 * eight drawing LVOs added; 1.3 adds the look hook (gfopengpu.library 0.7);
 * 1.4 patches every drawing call, 22 in all (gfopengpu.library 0.8); 1.5
 * the area fills, DrawEllipse and EraseRect too, 28 in all (0.14). */
#define OGFX_INTERFACE_VERSION  1
#define OGFX_INTERFACE_REVISION 5
#define OGFX_PATCHES_ALL 22               /* 1.4: the graphics.library calls GFOpenGfx owns */
#define OGFX_PATCHES_AREA 28              /* 1.5: and AreaMove, AreaDraw, AreaEllipse, AreaEnd, DrawEllipse, EraseRect */

/* The rest is gfopengfx.library 1.1's public header (libraries/opengfx.h), the
 * same names and layouts, so either header may come first. */
#ifndef LIBRARIES_OPENGFX_H
#define LIBRARIES_OPENGFX_H

#define OPENGFXLIB_NAME "gfopengfx.library"
#define OPENGFXLIB_VERSION 1
#define OPENGFXLIB_REVISION 1

#define OGFX_PROVIDER_ABI_V1 1

#define OGFX_STATUS_PATCHED  (1UL << 0)
#define OGFX_STATUS_ENABLED  (1UL << 1)
#define OGFX_STATUS_PROVIDER (1UL << 2)
#define OGFX_STATUS_AMICRADLE (1UL << 3)   /* AmiCradle Team boards found; Chip RAM drawing goes to the leaves */
#define OGFX_PATCHES 8                       /* the graphics.library calls GFOpenGfx 1.1 owned (1.4: OGFX_PATCHES_ALL) */

struct OGFXRectFillRequest {
    struct RastPort *rp;
    LONG x0, y0, x1, y1;
};

struct OGFXTextRequest {
    struct RastPort *rp;
    STRPTR text;
    ULONG length;
    LONG result;
};

struct OGFXBltBitMapRequest {
    struct BitMap *src;
    LONG sx, sy;
    struct BitMap *dst;
    LONG dx, dy;
    LONG width, height;
    ULONG minterm;
    ULONG mask;
    PLANEPTR temp;
    LONG result;
};

struct OGFXScrollRasterRequest {
    struct RastPort *rp;
    LONG dx, dy;
    LONG x0, y0, x1, y1;
};

struct OGFXTextLengthRequest {
    struct RastPort *rp;
    STRPTR text;
    ULONG length;
    LONG result;                       /* TextLength's WORD */
};

struct OGFXTextExtentRequest {
    struct RastPort *rp;
    STRPTR text;
    ULONG length;
    struct TextExtent *extent;
};

struct OGFXTextFitRequest {
    struct RastPort *rp;
    STRPTR text;
    ULONG length;
    struct TextExtent *extent;
    struct TextExtent *constraining;
    LONG direction;
    ULONG bit_width, bit_height;
    ULONG result;
};

struct OGFXBltTemplateRequest {
    PLANEPTR source;
    LONG sx, source_modulo;
    struct RastPort *rp;
    LONG dx, dy, width, height;
};

/* A provider returns non-zero only when it has reproduced graphics.library's
 * semantics exactly and GFOpenGfx must not call the original vector. The record
 * is copied, so the caller may keep it on the stack; owner must stay a stable
 * non-NULL token until it unregisters. size is sizeof the record the provider
 * was built with: one built with gfopengfx.library 1.0's header (the first four
 * calls) is taken, and the calls it has no room for go to graphics.library.
 * A NULL entry also means "not mine". */
struct OGFXProviderV1 {
    ULONG size;
    ULONG abi;
    APTR owner;
    APTR userdata;

    LONG (*rectfill)(APTR userdata, struct OGFXRectFillRequest *request);
    LONG (*text)(APTR userdata, struct OGFXTextRequest *request);
    LONG (*bltbitmap)(APTR userdata, struct OGFXBltBitMapRequest *request);
    LONG (*scrollraster)(APTR userdata, struct OGFXScrollRasterRequest *request);
    /* gfopengfx.library 1.1 */
    LONG (*textlength)(APTR userdata, struct OGFXTextLengthRequest *request);
    LONG (*textextent)(APTR userdata, struct OGFXTextExtentRequest *request);
    LONG (*textfit)(APTR userdata, struct OGFXTextFitRequest *request);
    LONG (*blttemplate)(APTR userdata, struct OGFXBltTemplateRequest *request);
};

/* sizeof a 1.0 provider record: the first four calls only. */
#define OGFX_PROVIDER_V1_0_SIZE ((ULONG)&((struct OGFXProviderV1 *)0)->textlength)

#endif /* LIBRARIES_OPENGFX_H */

/* ---- every drawing call (gfopengpu.library 0.8, GFOpenGfx interface 1.4) -------

   A provider built for 1.4 registers a struct OGFXProviderAll: the v1 record
   as its first member (abi OGFX_PROVIDER_ABI_V1, size the whole record),
   then one entry for each of the fourteen calls 1.4 adds. GFOpenGfx asks the
   provider first on each, as on the first eight; it returns non-zero only
   when it has drawn the call exactly as graphics.library would (and set
   result where the call has one). A shorter record (a v1 provider) is taken
   as before: the calls it has no room for go to graphics.library. Each
   request holds the call's arguments as graphics.library reads them: 16-bit
   coordinates, sizes and counts sign- or zero-extended; the pixel-array
   calls' coordinates whole. OGFX_Version() >= 1.4 says GFOpenGfx patches the
   fourteen, so a provider can stop patching them itself. */

struct OGFXBltPatternRequest {         /* BltPattern(rp, mask, xl, yl, maxx, maxy, bytecnt) */
    struct RastPort *rp;
    PLANEPTR mask;
    LONG x0, y0, x1, y1;
    LONG mask_bpr;
};

struct OGFXSetRastRequest {            /* SetRast(rp, pen) */
    struct RastPort *rp;
    ULONG pen;
};

struct OGFXDrawRequest {               /* Draw(rp, x, y) */
    struct RastPort *rp;
    LONG x, y;
};

struct OGFXPolyDrawRequest {           /* PolyDraw(rp, count, array) */
    struct RastPort *rp;
    LONG count;
    WORD *array;
};

struct OGFXPixelRequest {              /* WritePixel and ReadPixel(rp, x, y) */
    struct RastPort *rp;
    LONG x, y;
    LONG result;
};

struct OGFXBltBitMapRastPortRequest {  /* BltBitMapRastPort, and BltMaskBitMapRastPort with mask */
    struct BitMap *src;
    LONG sx, sy;
    struct RastPort *rp;
    LONG dx, dy, width, height;
    ULONG minterm;
    PLANEPTR mask;                     /* NULL for BltBitMapRastPort */
};

struct OGFXClipBlitRequest {           /* ClipBlit(srcrp, sx, sy, destrp, dx, dy, w, h, minterm) */
    struct RastPort *src_rp;
    LONG sx, sy;
    struct RastPort *rp;
    LONG dx, dy, width, height;
    ULONG minterm;
};

/* WriteChunkyPixels(rp, x0, y0, x1, y1, array, bytes_per_row);
 * WritePixelArray8 and ReadPixelArray8(rp, x0, y0, x1, y1, array, temp_rp);
 * WritePixelLine8 and ReadPixelLine8(rp, x0, y0, width, array, temp_rp),
 * where x1 and y1 are x0 + width - 1 and y0. */
struct OGFXPixelArrayRequest {
    struct RastPort *rp;
    LONG x0, y0, x1, y1;
    ULONG width;                       /* the line calls' width; 0 for the others */
    UBYTE *array;
    LONG bytes_per_row;                /* WriteChunkyPixels' only */
    struct RastPort *temp_rp;          /* the *8 calls' */
    LONG result;
};

struct OGFXProviderAll {
    struct OGFXProviderV1 v1;          /* size: sizeof(struct OGFXProviderAll) */
    LONG (*bltpattern)(APTR userdata, struct OGFXBltPatternRequest *request);
    LONG (*setrast)(APTR userdata, struct OGFXSetRastRequest *request);
    LONG (*draw)(APTR userdata, struct OGFXDrawRequest *request);
    LONG (*polydraw)(APTR userdata, struct OGFXPolyDrawRequest *request);
    LONG (*writepixel)(APTR userdata, struct OGFXPixelRequest *request);
    LONG (*readpixel)(APTR userdata, struct OGFXPixelRequest *request);
    LONG (*bltbitmaprastport)(APTR userdata, struct OGFXBltBitMapRastPortRequest *request);
    LONG (*bltmaskbitmaprastport)(APTR userdata, struct OGFXBltBitMapRastPortRequest *request);
    LONG (*clipblit)(APTR userdata, struct OGFXClipBlitRequest *request);
    LONG (*writechunkypixels)(APTR userdata, struct OGFXPixelArrayRequest *request);
    LONG (*writepixelarray8)(APTR userdata, struct OGFXPixelArrayRequest *request);
    LONG (*writepixelline8)(APTR userdata, struct OGFXPixelArrayRequest *request);
    LONG (*readpixelline8)(APTR userdata, struct OGFXPixelArrayRequest *request);
    LONG (*readpixelarray8)(APTR userdata, struct OGFXPixelArrayRequest *request);
};

/* ---- the look hook (gfopengpu.library 0.7, GFOpenGfx interface 1.3) -------------

   GFOpenGfx owns graphics.library's drawing and text patches, so a program
   that draws the system's look on RectFill and Text (GFOpenLook: window frames,
   title bars, the screen's bar, border scrollers) registers a look instead
   of patching them. There is one look at a time.

   - GFOpenGfx asks the look first, before a provider and before its own paths,
     on every RectFill and Text, whichever way it came (the patch or the LVO).
     The request holds the arguments as graphics.library reads them (16-bit
     coordinates and counts, sign-extended).
   - The look returns non-zero when it has drawn the call itself, and the call
     ends there (for Text it sets request->result); zero, and the call goes on
     as if there were no look. A NULL entry is "not mine".
   - The look may draw with graphics.library, RectFill and Text included: those
     calls come back through GFOpenGfx and to the look again, so it must decline
     its own (GFOpenLook keeps a flag while it draws).
   - It is called on the drawing task, any task, from C (the arguments on the
     stack, the result in D0; D0, D1, A0 and A1 are the look's to change). It
     must not wait for long: Intuition may hold its locks.
   - The record is copied, so it may be on the stack; owner must stay a stable
     non-NULL token until the look is taken out.
   - OGFX_RegisterLook answers 0 when another look is in, or while calls into a
     look taken out are still running; 1 when this look is in.
   - OGFX_UnregisterLook takes the look out (1) and calls stop reaching it at
     once. Calls already inside it run on: the owner waits until OGFX_Status()
     clears OGFX_STATUS_LOOK_BUSY before its code goes.
   - Registering a look changes no vector. A look reaches graphics.library's
     calls once OGFX_InstallPatches() has put GFOpenGfx's patches in (GFOpenRTG
     does; a look may ask for them itself). */

#define OGFX_LOOK_ABI_V1 1

#define OGFX_STATUS_LOOK      (1UL << 4)   /* a look is in */
#define OGFX_STATUS_LOOK_BUSY (1UL << 5)   /* calls are inside a look, in or just taken out */

struct OGFXLookV1 {
    ULONG size;                        /* sizeof(struct OGFXLookV1) */
    ULONG abi;                         /* OGFX_LOOK_ABI_V1 */
    APTR owner;
    APTR userdata;

    LONG (*rectfill)(APTR userdata, struct OGFXRectFillRequest *request);
    LONG (*text)(APTR userdata, struct OGFXTextRequest *request);
};

#endif

/* ---- end of gfx.h's text ---- */

/* 1.5's area calls, for a gfx.h of 1.4 included first */
#ifndef OGFX_PATCHES_AREA
#define OGFX_PATCHES_AREA 28
#endif

#include <exec/libraries.h>

/* graphics.library 47.101: the first with the dispatch; 47.102: the
 * provider chain and GFX_RegisterProviderPri */
#define GFXDISPATCH_LIB_VERSION  47
#define GFXDISPATCH_LIB_REVISION 101
#define GFXDISPATCH_CHAIN_LIB_REVISION 102

/* GFX_DispatchVersion(): the OGFX interface the dispatch keeps, 1.5, and
 * from 47.102 1.6, 1.5 with the provider chain */
#define GFXDISPATCH_INTERFACE_VERSION  1
#define GFXDISPATCH_INTERFACE_REVISION 6
#define GFXDISPATCH_CHAIN_INTERFACE ((1UL << 16) | 6)   /* GFX_DispatchVersion() at least this: the chain */

/* The provider chain (47.102): how many providers at once, and the
 * priorities in use (higher is asked first) */
#define GFXDISPATCH_PROVIDERS 4
#define GFXDISPATCH_PRI_RTG      20     /* an RTG system's provider (GFOpenRTG): its own bitmaps */
#define GFXDISPATCH_PRI_DEFAULT  0      /* GFX_RegisterProvider, and gfopengpu's OGFX_RegisterProvider */
#define GFXDISPATCH_PRI_FAST   (-20)    /* gfopengpu.library's planar fast paths: what the others decline */

/* The calls' LVOs */
#define GFXDISPATCH_LVO_VERSION             (-1062)
#define GFXDISPATCH_LVO_REGISTERPROVIDER    (-1068)
#define GFXDISPATCH_LVO_UNREGISTERPROVIDER  (-1074)
#define GFXDISPATCH_LVO_REGISTERLOOK        (-1080)
#define GFXDISPATCH_LVO_UNREGISTERLOOK      (-1086)
#define GFXDISPATCH_LVO_STATUS              (-1092)
#define GFXDISPATCH_LVO_REGISTERPROVIDERPRI (-1098)   /* 47.102 */
#define GFXDISPATCH_NEGSIZE                 1092   /* lib_NegSize at least this */
#define GFXDISPATCH_CHAIN_NEGSIZE           1098   /* and this for the chain (47.102) */

#define GFX_DISPATCH_AVAILABLE(gfxbase) \
    ((gfxbase) && ((struct Library *)(gfxbase))->lib_NegSize >= GFXDISPATCH_NEGSIZE && \
     (((struct Library *)(gfxbase))->lib_Version > GFXDISPATCH_LIB_VERSION || \
      (((struct Library *)(gfxbase))->lib_Version == GFXDISPATCH_LIB_VERSION && \
       ((struct Library *)(gfxbase))->lib_Revision >= GFXDISPATCH_LIB_REVISION)))

#define GFX_DISPATCH_CHAIN_AVAILABLE(gfxbase) \
    ((gfxbase) && ((struct Library *)(gfxbase))->lib_NegSize >= GFXDISPATCH_CHAIN_NEGSIZE && \
     (((struct Library *)(gfxbase))->lib_Version > GFXDISPATCH_LIB_VERSION || \
      (((struct Library *)(gfxbase))->lib_Version == GFXDISPATCH_LIB_VERSION && \
       ((struct Library *)(gfxbase))->lib_Revision >= GFXDISPATCH_CHAIN_LIB_REVISION)))

#endif /* GRAPHICS_GFXDISPATCH_H */
