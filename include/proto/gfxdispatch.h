/* SPDX-License-Identifier: MIT */
/* GFOpenGfx graphics.library 47.101 and up: the dispatch calls, through GfxBase
 * (graphics/gfxdispatch.h). Check GFX_DISPATCH_AVAILABLE(GfxBase) before
 * the first call, and GFX_DISPATCH_CHAIN_AVAILABLE(GfxBase) before
 * GFX_RegisterProviderPri. Copyright (c) 2026 AmiCradle Team. MIT. */
#ifndef PROTO_GFXDISPATCH_H
#define PROTO_GFXDISPATCH_H

#include <proto/graphics.h>
#include <clib/gfxdispatch_protos.h>

#if defined(__GNUC__) && defined(__mc68000__) && !defined(__OPENNDK_NO_INLINE__)
#include <inline/gfxdispatch.h>
#endif

#endif
