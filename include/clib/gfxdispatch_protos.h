/* SPDX-License-Identifier: MIT */
/* GFOpenGfx graphics.library 47.101 and up: the dispatch calls' prototypes
 * (graphics/gfxdispatch.h). Copyright (c) 2026 AmiCradle Team. MIT. */
#ifndef CLIB_GFXDISPATCH_PROTOS_H
#define CLIB_GFXDISPATCH_PROTOS_H

#include <graphics/gfxdispatch.h>

#ifdef __cplusplus
extern "C" {
#endif

ULONG GFX_DispatchVersion(VOID);
LONG GFX_RegisterProvider(struct OGFXProviderV1 *provider);
LONG GFX_UnregisterProvider(APTR owner);
LONG GFX_RegisterLook(struct OGFXLookV1 *look);
LONG GFX_UnregisterLook(APTR owner);
ULONG GFX_DispatchStatus(VOID);
LONG GFX_RegisterProviderPri(struct OGFXProviderV1 *provider, LONG pri);   /* 47.102 */

#ifdef __cplusplus
}
#endif

#endif
