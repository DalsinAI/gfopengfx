/* SPDX-License-Identifier: MIT */
/* gfopenoverlay.library 1.0's prototypes (libraries/gfopenoverlay.h). Copyright (c) 2026 AmiCradle Team. MIT. */
#ifndef CLIB_GFOPENOVERLAY_PROTOS_H
#define CLIB_GFOPENOVERLAY_PROTOS_H

#include <libraries/gfopenoverlay.h>

struct Window;

#ifdef __cplusplus
extern "C" {
#endif

APTR OpenOverlay(struct Window *window, const struct TagItem *tags);
struct LayerOverlayPlace *OverlayPlace(APTR overlay);
LONG SetOverlayAttrs(APTR overlay, const struct TagItem *tags);
VOID CloseOverlay(APTR overlay);

#ifdef __cplusplus
}
#endif

#endif
