/* SPDX-License-Identifier: MIT */
/* GFOpenGfx graphics.library 47.101: a provider and a look built against
 * the dispatch's SDK headers (include/graphics/gfxdispatch.h,
 * proto/gfxdispatch.h), as GFOpenRTG and GFOpenLook would register them.
 * library/graphics/build-amiga.sh compiles it alone and with gfopengpu's
 * include/gfopengpu/gfx.h first, and checks that each of the six calls is
 * its LVO. Not run: the host test (tests/m68k/test_gfx_dispatch.sh) runs
 * the dispatch itself. Copyright (c) 2026 AmiCradle Team. MIT. */
#include <exec/types.h>
#include <graphics/gfxbase.h>
#include <proto/gfxdispatch.h>

struct GfxBase *GfxBase;

static LONG my_rectfill(APTR userdata, struct OGFXRectFillRequest *r)
{
    (void)userdata;
    return r->rp && r->x0 > r->x1;
}

static LONG my_writepixel(APTR userdata, struct OGFXPixelRequest *r)
{
    (void)userdata;
    r->result = 0;
    return r->x < 0;
}

static LONG my_look_text(APTR userdata, struct OGFXTextRequest *r)
{
    (void)userdata;
    r->result = 0;
    return r->length == 0;
}

static struct OGFXProviderAll provider;
static UBYTE owner;

ULONG gfxdispatch_sdk_check(void)
{
    struct OGFXLookV1 look;
    ULONG s = 0;
    if (!GFX_DISPATCH_AVAILABLE(GfxBase) || GFX_DispatchVersion() < ((1UL << 16) | 5))
        return 0;
    provider.v1.size = sizeof(provider);
    provider.v1.abi = OGFX_PROVIDER_ABI_V1;
    provider.v1.owner = &owner;
    provider.v1.rectfill = my_rectfill;
    provider.writepixel = my_writepixel;
    look.size = sizeof(look);
    look.abi = OGFX_LOOK_ABI_V1;
    look.owner = &owner;
    look.userdata = 0;
    look.rectfill = 0;
    look.text = my_look_text;
    s += GFX_RegisterProvider(&provider.v1);
    s += GFX_RegisterLook(&look);
    s += GFX_DispatchStatus() & (OGFX_STATUS_PROVIDER | OGFX_STATUS_LOOK);
    s += GFX_UnregisterLook(&owner);
    s += GFX_UnregisterProvider(&owner);
    return s + OGFX_PATCHES_AREA + GFXDISPATCH_LVO_STATUS;
}
