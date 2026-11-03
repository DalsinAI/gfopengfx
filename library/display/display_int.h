/* SPDX-License-Identifier: MIT */
/* OpenGfx display: what the core's files share. Copyright (c) 2026 AmiCradle
 * Team. MIT. Lean C++: no Amiga headers, no globals, no runtime. */
#ifndef OPENGFX_DISPLAY_INT_H
#define OPENGFX_DISPLAY_INT_H

#include "display.h"

namespace gd {

/* The display's own state, in gfx_private_of(GfxBase)->area[GFX_AREA_DISPLAY]
 * (the library has no .bss). */
struct assoc {                          /* GfxAssociate's pairs */
    assoc *next;
    const void *key;
    gd_extnode *node;
};
/* Six monitors: default, NTSC and PAL, built in; Multiscan, DblNTSC and
 * DblPAL, whose modes are there while their driver's MonitorSpec is on
 * GfxBase->MonitorList (display_db.cpp). Each has the native keys its
 * driver has: the built-in ones 30, DblNTSC and DblPAL 16, Multiscan 24. */
enum { NHASH = 32, NMONITORS = 6, NBUILTIN = 3, NKEYS = 30, NMODES = 3 * 30 + 2 * 16 + 24 };
/* GfxBase's semaphore pointers, each to one of state::sems. Programs outside
 * graphics take them (a monitor driver run by LoadMonDrvs takes
 * MonitorListSemaphore before it adds its MonitorSpec), so none may be NULL:
 * ObtainSemaphore(NULL) reads low memory as a semaphore that some other task
 * owns, and waits for ever. */
enum { SEM_LASTCHANCE, SEM_ACTIVIEWCPR, SEM_MONITORLIST, SEM_HASHTABLE, NSEMS };

struct state {
    gd_copinit *copinit;                /* chip RAM; also GfxBase->copinit */
    gd_simplesprite *sprites[8];        /* GfxBase->SimpleSprites */
    gd_monitorspec *monitors[NBUILTIN]; /* default, ntsc, pal */
    assoc *hash[NHASH];                 /* GfxBase->hash_table */
    uint32_t modeids[NMODES];           /* DisplayInfoHandles point here */
    uint16_t fmode;                     /* what copinit loads into FMODE */
    uint16_t spritewords;               /* 1, 2 or 4: the sprites' fetch width */
    bool hw;                            /* the display is ours: write the registers */
    gfx_semaphore sems[NSEMS];          /* the SignalSemaphores GfxBase points at (SEM_ below) */
};

static inline gfx_base *base(gfx_env *env) { return env->base; }
static inline state *st(gfx_env *env)
{
    return static_cast<state *>(gfx_private_of(env->base)->area[GFX_AREA_DISPLAY]);
}
/* A custom chip register write, dropped when the display is the OS's
 * (gfopengfx.library opened next to it for the ABI tests) */
static inline void put(gfx_env *env, uint32_t reg, uint16_t v)
{
    state *s = st(env);
    if (s && s->hw) env->chip_write(env->ctx, reg, v);
}
static inline void *alloc(gfx_env *env, uint32_t size, uint32_t flags) { return env->alloc(env->ctx, size, flags); }
static inline void release(gfx_env *env, void *p, uint32_t size)
{
    if (p) env->free(env->ctx, p, size);
}
static inline bool aga(gfx_env *env) { return (base(env)->chiprevbits0 & GFX_GFXF_AA_LISA) != 0; }
static inline bool ecs_denise(gfx_env *env)
{
    return (base(env)->chiprevbits0 & (GFX_GFXF_HR_DENISE | GFX_GFXF_AA_LISA)) != 0;
}

/* utility.library's NextTagItem, done here so the core needs nothing of
 * utility: TAG_DONE ends, TAG_IGNORE skips one, TAG_MORE continues at
 * ti_Data, TAG_SKIP skips ti_Data + 1. */
gfx_tagitem *next_tag(const gfx_tagitem **state);
uint32_t tag_data(const gfx_tagitem *tags, uint32_t tag, uint32_t def, bool *found = nullptr);

/* exec lists */
void list_init(gfx_list *l);
void add_tail(gfx_list *l, gfx_node *n);
void remove(gfx_node *n);

void zero(void *p, uint32_t n);

/* WaitTOF's node in GfxBase->TOF_WaitQ: the vertical blank takes it out,
 * clears task and signals SIGF_SINGLE */
struct tof_waiter {
    gfx_node node;
    void *volatile task;
};
enum { SIGF_SINGLE = 0x10 };

/* the View's monitor geometry */
struct geom {
    int total_rows;                     /* lines a field shows: 0 .. total_rows-1 */
    int min_row;                        /* the first line bitplanes can show */
    bool pal;
    int scan;                           /* SCAN_: how its modes use the chips */
};
/* A 31 kHz monitor's modes (OS 3.2.3's copper lists, measured): every line
 * progressive, a ViewPort's LACE meaning one row a line ("No Flicker") and
 * no LACE each row shown on two lines (FMODE's BSCAN2); sprites scan doubled
 * (SSCAN2). DblNTSC's and DblPAL's bitplanes run a resolution faster than
 * the mode says (Low Res on hires bitplanes); Multiscan's as it says. */
enum { SCAN_NONE, SCAN_DBL, SCAN_MULTI };
int scan_of(const gd_monitorspec *ms);
geom view_geom(gfx_env *env, gd_view *view);
gd_monitorspec *monitor_of_view(gfx_env *env, gd_view *view);

/* the legal horizontal span in lores pixels: DDFSTRT 0x18 .. DDFSTOP 0xD8
 * with room for a whole fetch block of scroll on either side (ours). */
constexpr int LEGAL_X0 = 0x51, LEGAL_X1 = 0x1C1, VIEW_X = 0x81, VIEW_Y = 0x2C;

/* rewrite the hardware lists' copy of vp's first (register and colour)
 * block after its DspIns changed */
void patch_vp(gfx_env *env, gd_viewport *vp, bool colours_only);
/* copinit's sprite pointer for sprite n */
void set_sprite_ptr(gfx_env *env, int n, const uint16_t *data);
void set_fmode(gfx_env *env);

} // namespace gd

#endif
