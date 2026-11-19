/* SPDX-License-Identifier: MIT */
/* gfopenoverlay.library 1.0: Overlay, pictures the host puts on the screen, scaled by the display
 * hardware (gfosdesign gfos/libraries/gfopenlayout-overlay). A box in a window whose pixels come
 * from the host (a guest core's frames, GL, a video, a camera), never from the Amiga. The program
 * that owns the window opens the overlay and hands its place record to the host; the compositor
 * (layers.library 47.111 and up) keeps the record right as the window moves, sizes, is covered and
 * uncovered; the display scales the picture into the box. Nothing per frame on the Amiga. Any
 * screen: RTG or AGA (an Intuition screen of any mode).
 *
 *   ov = OpenOverlay(window, tags)        the box in the window's inner area (default: all of it)
 *   place = OverlayPlace(ov)              the place record, for the host
 *   SetOverlayAttrs(ov, tags)             a new box, picture size or aspect
 *   CloseOverlay(ov)                      after the host has stopped
 *
 * Copyright (c) 2026 AmiCradle Team. MIT. */
#ifndef LIBRARIES_GFOPENOVERLAY_H
#define LIBRARIES_GFOPENOVERLAY_H

#include <exec/types.h>
#include <utility/tagitem.h>
#include <graphics/gfopenlayers.h>      /* OVA_ tags, OVASPECT_, struct LayerOverlayPlace, OVPF_ */

#define GFOPENOVERLAY_NAME    "gfopenoverlay.library"
#define GFOPENOVERLAY_VERSION 1

/* In OpenOverlay and SetOverlayAttrs, OVA_Left, OVA_Top, OVA_Width and OVA_Height are in the
 * window's inner area (inside its borders), not the whole window as layers.library's own call has
 * them. Without them the box is the whole inner area, and SetOverlayAttrs with none of them puts
 * it back to the inner area (after the window was sized, say). OVA_PicWidth, OVA_PicHeight and
 * OVA_Aspect go to the record as they are. */

#define LVO_GFOpenOverlay_OpenOverlay      (-30)
#define LVO_GFOpenOverlay_OverlayPlace     (-36)
#define LVO_GFOpenOverlay_SetOverlayAttrs  (-42)
#define LVO_GFOpenOverlay_CloseOverlay     (-48)

#endif
