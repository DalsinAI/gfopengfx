/* SPDX-License-Identifier: MIT */
/* GFOpenGfx graphics.library 47.101 and up: GCC inline calls for the dispatch
 * (graphics/gfxdispatch.h), in the style of the stove's inline/graphics.h.
 * Copyright (c) 2026 AmiCradle Team. MIT. */
#ifndef INLINE_GFXDISPATCH_H
#define INLINE_GFXDISPATCH_H

#ifndef CLIB_GFXDISPATCH_PROTOS_H
#include <clib/gfxdispatch_protos.h>
#endif

#ifndef GRAPHICS_BASE_NAME
#define GRAPHICS_BASE_NAME GfxBase
#endif

#ifndef __GFXDISPATCH_FPCLOBBER
#ifdef __HAVE_68881__
#define __GFXDISPATCH_FPCLOBBER "fp0", "fp1",
#else
#define __GFXDISPATCH_FPCLOBBER
#endif
#endif

#define __GFXDISPATCH_CALL0(lvo, type)                                            \
    register ULONG _d0 __asm("d0");                                             \
    register ULONG _d1 __asm("d1");                                             \
    register ULONG _a0 __asm("a0");                                             \
    register ULONG _a1 __asm("a1");                                             \
    register void *_a6 __asm("a6") = __base;                                    \
    __asm__ __volatile__ ("jsr " #lvo "(%/a6)"                                  \
        : "=r" (_d0), "=r" (_d1), "=r" (_a0), "=r" (_a1)                        \
        : "r" (_a6)                                                             \
        : __GFXDISPATCH_FPCLOBBER "cc", "memory");                               \
    (void)_d1; (void)_a0; (void)_a1;                                            \
    return (type)_d0

#define __GFXDISPATCH_CALL1(lvo, type, arg)                                       \
    register ULONG _d0 __asm("d0");                                             \
    register ULONG _d1 __asm("d1");                                             \
    register ULONG _a0 __asm("a0") = (ULONG)(arg);                              \
    register ULONG _a1 __asm("a1");                                             \
    register void *_a6 __asm("a6") = __base;                                    \
    __asm__ __volatile__ ("jsr " #lvo "(%/a6)"                                  \
        : "=r" (_d0), "=r" (_d1), "+r" (_a0), "=r" (_a1)                        \
        : "r" (_a6)                                                             \
        : __GFXDISPATCH_FPCLOBBER "cc", "memory");                               \
    (void)_d1; (void)_a0; (void)_a1;                                            \
    return (type)_d0

#define __GFXDISPATCH_CALL2(lvo, type, arg, darg)                                 \
    register ULONG _d0 __asm("d0") = (ULONG)(darg);                             \
    register ULONG _d1 __asm("d1");                                             \
    register ULONG _a0 __asm("a0") = (ULONG)(arg);                              \
    register ULONG _a1 __asm("a1");                                             \
    register void *_a6 __asm("a6") = __base;                                    \
    __asm__ __volatile__ ("jsr " #lvo "(%/a6)"                                  \
        : "+r" (_d0), "=r" (_d1), "+r" (_a0), "=r" (_a1)                        \
        : "r" (_a6)                                                             \
        : __GFXDISPATCH_FPCLOBBER "cc", "memory");                               \
    (void)_d1; (void)_a0; (void)_a1;                                            \
    return (type)_d0

static __inline__ ULONG __gfxdispatch_GFX_DispatchVersion(void *__base)
{ __GFXDISPATCH_CALL0(-1062, ULONG); }
static __inline__ LONG __gfxdispatch_GFX_RegisterProvider(void *__base, struct OGFXProviderV1 *provider)
{ __GFXDISPATCH_CALL1(-1068, LONG, provider); }
static __inline__ LONG __gfxdispatch_GFX_UnregisterProvider(void *__base, APTR owner)
{ __GFXDISPATCH_CALL1(-1074, LONG, owner); }
static __inline__ LONG __gfxdispatch_GFX_RegisterLook(void *__base, struct OGFXLookV1 *look)
{ __GFXDISPATCH_CALL1(-1080, LONG, look); }
static __inline__ LONG __gfxdispatch_GFX_UnregisterLook(void *__base, APTR owner)
{ __GFXDISPATCH_CALL1(-1086, LONG, owner); }
static __inline__ ULONG __gfxdispatch_GFX_DispatchStatus(void *__base)
{ __GFXDISPATCH_CALL0(-1092, ULONG); }
static __inline__ LONG __gfxdispatch_GFX_RegisterProviderPri(void *__base, struct OGFXProviderV1 *provider, LONG pri)
{ __GFXDISPATCH_CALL2(-1098, LONG, provider, pri); }

#define GFX_DispatchVersion() __gfxdispatch_GFX_DispatchVersion((GRAPHICS_BASE_NAME))
#define GFX_RegisterProvider(provider) \
    __gfxdispatch_GFX_RegisterProvider((GRAPHICS_BASE_NAME), (struct OGFXProviderV1 *)(provider))
#define GFX_UnregisterProvider(owner) __gfxdispatch_GFX_UnregisterProvider((GRAPHICS_BASE_NAME), (APTR)(owner))
#define GFX_RegisterLook(look) __gfxdispatch_GFX_RegisterLook((GRAPHICS_BASE_NAME), (struct OGFXLookV1 *)(look))
#define GFX_UnregisterLook(owner) __gfxdispatch_GFX_UnregisterLook((GRAPHICS_BASE_NAME), (APTR)(owner))
#define GFX_DispatchStatus() __gfxdispatch_GFX_DispatchStatus((GRAPHICS_BASE_NAME))
#define GFX_RegisterProviderPri(provider, pri) \
    __gfxdispatch_GFX_RegisterProviderPri((GRAPHICS_BASE_NAME), (struct OGFXProviderV1 *)(provider), (LONG)(pri))

#endif
