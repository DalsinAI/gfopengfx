/* SPDX-License-Identifier: MIT */
/* gfopenoverlay.library 1.0: GCC inline calls (libraries/gfopenoverlay.h).
 * Copyright (c) 2026 AmiCradle Team. MIT. */
#ifndef INLINE_GFOPENOVERLAY_H
#define INLINE_GFOPENOVERLAY_H

#ifndef CLIB_GFOPENOVERLAY_PROTOS_H
#include <clib/gfopenoverlay_protos.h>
#endif

#ifndef GFOPENOVERLAY_BASE_NAME
#define GFOPENOVERLAY_BASE_NAME GFOpenOverlayBase
#endif

#define __GFOVL_CALL(lvo, a0v, a1v)                                                 \
    ({ register ULONG _d0 __asm("d0");                                              \
       register ULONG _d1 __asm("d1");                                              \
       register ULONG _a0 __asm("a0") = (ULONG)(a0v);                               \
       register ULONG _a1 __asm("a1") = (ULONG)(a1v);                               \
       register void *_a6 __asm("a6") = (void *)(GFOPENOVERLAY_BASE_NAME);          \
       __asm__ __volatile__ ("jsr " #lvo "(%%a6)"                                   \
           : "=r" (_d0), "=r" (_d1), "+r" (_a0), "+r" (_a1)                         \
           : "r" (_a6)                                                              \
           : "cc", "memory");                                                       \
       (void)_d1; _d0; })

#define OpenOverlay(window, tags)       ((APTR)__GFOVL_CALL(-30, (window), (tags)))
#define OverlayPlace(ov)                ((struct LayerOverlayPlace *)__GFOVL_CALL(-36, (ov), 0))
#define SetOverlayAttrs(ov, tags)       ((LONG)__GFOVL_CALL(-42, (ov), (tags)))
#define CloseOverlay(ov)                ((void)__GFOVL_CALL(-48, (ov), 0))

#endif
