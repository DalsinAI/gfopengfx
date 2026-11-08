/* SPDX-License-Identifier: MIT */
/* GFOpenGfx display: extended nodes, monitors, the display database,
 * VideoControl, and the display's start. Copyright (c) 2026 AmiCradle
 * Team. MIT.
 *
 * Clean-room: from graphics.library's autodocs (GfxNew, GfxFree,
 * GfxAssociate, GfxLookUp, OpenMonitor, CloseMonitor, FindDisplayInfo,
 * NextDisplayInfo, GetDisplayInfoData, GetVPModeID, ModeNotAvailable,
 * BestModeIDA, CoerceMode, VideoControl) and the NDK 3.2 headers
 * (graphics/modeid.h, displayinfo.h, monitor.h, videocontrol.h); never
 * from Kickstart's code. Lean C++: no Amiga headers, no globals; the
 * tables are constants.
 *
 * The database is ours: the default, NTSC and PAL monitors times the 30
 * native keys of modeid.h, each with its NotAvailable from the chips; and
 * Multiscan, DblNTSC and DblPAL with the keys, records and names OS 3.2.3's
 * drivers give (measured with tests/MonitorProbe.c on 3 November 2026),
 * there while the driver's own MonitorSpec is on GfxBase->MonitorList (a
 * driver adds it there itself). Their timing (rows, colour clocks, first
 * row, BEAMCON0 and the SpecialMonitor's sync and blank) is the driver's:
 * MonitorInfo reads it from that MonitorSpec. A driver's own records (its
 * private -744 and -750 calls, measured with tests/DriverCallLog.c on 8
 * November 2026) come first: GfxBase->DisplayInfoDataBase is a tree it
 * links them into itself. */
#include "display_int.h"

using namespace gd;

namespace {

enum : uint8_t { K_LACE = 1, K_HAM = 2, K_EHB = 4, K_DPF = 8, K_DPF2 = 16 };
enum : uint8_t { NEED_OCS = 0, NEED_ECS = 1, NEED_AGA = 2 };
/* on: the monitors that have the key (ON_ below) */
enum : uint8_t { ON_STD = 1, ON_DBL = 2, ON_MULTI = 4, ON_ALL = 7 };
struct keydef {
    uint16_t key;
    uint8_t res;                        /* 0 lores, 1 hires, 2 superhires */
    uint8_t flags;
    uint8_t need;
    uint8_t on;
};
constexpr keydef keys[NKEYS] = {
    { 0x0000, 0, 0, NEED_OCS, ON_ALL },      { 0x0004, 0, K_LACE, NEED_OCS, ON_ALL },
    { 0x0080, 0, K_EHB, NEED_OCS, ON_ALL },  { 0x0084, 0, K_EHB | K_LACE, NEED_OCS, ON_ALL },
    { 0x0400, 0, K_DPF, NEED_OCS, ON_STD },  { 0x0404, 0, K_DPF | K_LACE, NEED_OCS, ON_ALL },
    { 0x0440, 0, K_DPF | K_DPF2, NEED_OCS, ON_STD }, { 0x0444, 0, K_DPF | K_DPF2 | K_LACE, NEED_OCS, ON_ALL },
    { 0x0800, 0, K_HAM, NEED_OCS, ON_ALL },  { 0x0804, 0, K_HAM | K_LACE, NEED_OCS, ON_ALL },
    { 0x8000, 1, 0, NEED_OCS, ON_ALL },      { 0x8004, 1, K_LACE, NEED_OCS, ON_ALL },
    { 0x8020, 2, 0, NEED_ECS, ON_STD | ON_MULTI }, { 0x8024, 2, K_LACE, NEED_ECS, ON_STD | ON_MULTI },
    { 0x8080, 1, K_EHB, NEED_AGA, ON_ALL },  { 0x8084, 1, K_EHB | K_LACE, NEED_AGA, ON_ALL },
    { 0x80A0, 2, K_EHB, NEED_AGA, ON_STD | ON_MULTI }, { 0x80A4, 2, K_EHB | K_LACE, NEED_AGA, ON_STD | ON_MULTI },
    { 0x8400, 1, K_DPF, NEED_OCS, ON_STD },  { 0x8404, 1, K_DPF | K_LACE, NEED_OCS, ON_ALL },
    { 0x8420, 2, K_DPF, NEED_ECS, ON_STD },  { 0x8424, 2, K_DPF | K_LACE, NEED_ECS, ON_STD | ON_MULTI },
    { 0x8440, 1, K_DPF | K_DPF2, NEED_OCS, ON_STD }, { 0x8444, 1, K_DPF | K_DPF2 | K_LACE, NEED_OCS, ON_ALL },
    { 0x8460, 2, K_DPF | K_DPF2, NEED_ECS, ON_STD }, { 0x8464, 2, K_DPF | K_DPF2 | K_LACE, NEED_ECS, ON_STD | ON_MULTI },
    { 0x8800, 1, K_HAM, NEED_AGA, ON_ALL },  { 0x8804, 1, K_HAM | K_LACE, NEED_AGA, ON_ALL },
    { 0x8820, 2, K_HAM, NEED_AGA, ON_STD | ON_MULTI }, { 0x8824, 2, K_HAM | K_LACE, NEED_AGA, ON_STD | ON_MULTI },
};
constexpr uint32_t monids[NMONITORS] = { GD_DEFAULT_MONITOR_ID, GD_NTSC_MONITOR_ID, GD_PAL_MONITOR_ID,
                                         0x00031000, 0x00091000, 0x000A1000 };   /* VGA (Multiscan), DBLNTSC, DBLPAL */
const char *const monnames[NMONITORS] = { "default.monitor", "ntsc.monitor", "pal.monitor",
                                          "multiscan.monitor", "DblNTSC.monitor", "DblPAL.monitor" };
constexpr uint8_t monon[NMONITORS] = { ON_STD, ON_STD, ON_STD, ON_MULTI, ON_DBL, ON_DBL };
const char *const resnames[3] = { "Low Res", "High Res", "Super-High Res" };

/* What OS 3.2.3's Multiscan, DblNTSC and DblPAL drivers' records say that
 * their MonitorSpecs do not (MonitorProbe): the view position and its
 * range's far corner, the vertical resolution (ticks) of a No Flicker mode,
 * the nominal rows of the others, the overscan's extra rows, the preferred
 * mode and the names' prefix. */
struct mon31 {
    int16_t posx, posy, rx1, ry1, vres, rows, extra;
    uint32_t pref;
    const char *name;
};
const mon31 mons31[3] = {
    { 83, 25, 85, 25, 22, 240, 0, 0x00039024, "MULTISCAN:" },
    { 91, 50, 101, 77, 26, 200, 13, 0x00099004, "DBLNTSC:" },
    { 91, 42, 101, 61, 22, 256, 9, 0x000A9004, "DBLPAL:" },
};

struct mode {
    int mon;                            /* monids' index */
    const keydef *k;
    uint32_t id;
};

int ncmp(const char *a, const char *b);

/* a monitor's MonitorSpec: a built-in's, or the driver's on MonitorList
 * (null while it has not run) */
gd_monitorspec *mspec(gfx_env *env, int mon)
{
    if (mon < NBUILTIN) return st(env)->monitors[mon];
    for (gfx_node *n = base(env)->monitorlist.head; n->succ; n = n->succ)
        if (n->name && !ncmp(n->name, monnames[mon])) return reinterpret_cast<gd_monitorspec *>(n);
    return nullptr;
}

bool find_mode(gfx_env *env, uint32_t id, mode &m)
{
    for (int i = 0; i < NMONITORS; i++)
        if ((id & GD_MONITOR_ID_MASK) == monids[i] && mspec(env, i))
            for (int j = 0; j < NKEYS; j++)
                if ((id & ~GD_MONITOR_ID_MASK) == keys[j].key && (keys[j].on & monon[i])) {
                    m = { i, &keys[j], id };
                    return true;
                }
    return false;
}

/* a driver mode's bitplanes: 0 lores, 1 hires, 2 superhires; DblNTSC's and
 * DblPAL's a resolution faster than the key says */
int bres(const mode &m) { return m.k->res + (monon[m.mon] == ON_DBL ? 1 : 0); }

/* a driver mode's properties, as OS 3.2.3's records have them: every one
 * with programmed beam, sprites, double buffering; a mode without
 * LACE (each row on two lines) AA and SCANDBL; a No Flicker one ECS, or AA
 * for HAM and EHB on bitplanes faster than lores; the Workbench's, without
 * HAM, EHB or dual playfield (Multiscan: on superhires only) */
uint32_t props31(const mode &m)
{
    const uint8_t f = m.k->flags;
    const int r = bres(m);
    uint32_t p = GD_DIPF_IS_SPRITES | GD_DIPF_IS_DRAGGABLE | GD_DIPF_IS_BEAMSYNC | GD_DIPF_IS_SPRITES_ATT
                 | GD_DIPF_IS_SPRITES_CHNG_RES | GD_DIPF_IS_SPRITES_BORDER | GD_DIPF_IS_SPRITES_CHNG_BASE
                 | GD_DIPF_IS_SPRITES_CHNG_PRI | GD_DIPF_IS_DBUFFER | GD_DIPF_IS_PROGBEAM;
    if (f & K_HAM) p |= GD_DIPF_IS_HAM;
    if (f & K_EHB) p |= GD_DIPF_IS_EXTRAHALFBRITE;
    if (f & K_DPF) p |= GD_DIPF_IS_DUALPF;
    if (f & K_DPF2) p |= GD_DIPF_IS_PF2PRI;
    if (m.mon == 5) p |= GD_DIPF_IS_PAL;
    if (!(f & (K_HAM | K_EHB | K_DPF)) && (monon[m.mon] == ON_DBL || r == 2)) p |= GD_DIPF_IS_WB;
    if (!(f & K_LACE)) p |= GD_DIPF_IS_AA | GD_DIPF_IS_SCANDBL;
    else p |= (f & (K_HAM | K_EHB)) && r ? GD_DIPF_IS_AA : GD_DIPF_IS_ECS;
    return p;
}

bool mon_pal(gfx_env *env, int mon)
{
    if (mon == 2) return true;
    if (mon == 1) return false;
    return (base(env)->displayflags & GFX_PAL) != 0;
}

uint32_t avail(gfx_env *env, const mode &m)
{
    uint8_t c = base(env)->chiprevbits0;
    if (m.mon >= NBUILTIN) {
        uint32_t p = props31(m);
        if ((p & GD_DIPF_IS_AA) && !(c & GFX_GFXF_AA_LISA)) return GD_DI_AVAIL_NOCHIPS;
        if (!(c & (GFX_GFXF_HR_DENISE | GFX_GFXF_AA_LISA))) return GD_DI_AVAIL_NOCHIPS;
        return 0;
    }
    if (m.k->need == NEED_ECS && !(c & (GFX_GFXF_HR_DENISE | GFX_GFXF_AA_LISA))) return GD_DI_AVAIL_NOCHIPS;
    if (m.k->need == NEED_AGA && !(c & GFX_GFXF_AA_LISA)) return GD_DI_AVAIL_NOCHIPS;
    return 0;
}

uint32_t props(gfx_env *env, const mode &m)
{
    if (m.mon >= NBUILTIN) return props31(m);
    uint32_t p = GD_DIPF_IS_SPRITES | GD_DIPF_IS_DRAGGABLE | GD_DIPF_IS_BEAMSYNC | GD_DIPF_IS_DBUFFER
                 | GD_DIPF_IS_SPRITES_ATT;
    uint8_t f = m.k->flags;
    if (f & K_LACE) p |= GD_DIPF_IS_LACE;
    if (f & K_HAM) p |= GD_DIPF_IS_HAM;
    if (f & K_EHB) p |= GD_DIPF_IS_EXTRAHALFBRITE;
    if (f & K_DPF) p |= GD_DIPF_IS_DUALPF;
    if (f & K_DPF2) p |= GD_DIPF_IS_PF2PRI;
    if (!(f & (K_HAM | K_EHB | K_DPF))) p |= GD_DIPF_IS_WB;
    if (m.k->need == NEED_ECS) p |= GD_DIPF_IS_ECS;
    if (m.k->need == NEED_AGA) p |= GD_DIPF_IS_AA;
    if (mon_pal(env, m.mon)) p |= GD_DIPF_IS_PAL;
    if (base(env)->displayflags & GFX_GENLOC) p |= GD_DIPF_IS_GENLOCK;
    if (aga(env))
        p |= GD_DIPF_IS_SPRITES_CHNG_RES | GD_DIPF_IS_SPRITES_BORDER | GD_DIPF_IS_SPRITES_CHNG_BASE
             | GD_DIPF_IS_SPRITES_CHNG_PRI;
    return p;
}

int maxdepth(gfx_env *env, const mode &m)
{
    uint8_t f = m.k->flags;
    /* AGA: 8 planes in every resolution (hires at 2x, superhires at 4x:
     * display_view.cpp), 6 for EHB */
    if (aga(env)) return (f & K_EHB) ? 6 : 8;
    if (m.k->res == 2) return 2;        /* OCS and ECS: the 1x fetch */
    if (m.k->res == 1) return 4;
    return (f & (K_HAM | K_EHB | K_DPF)) ? 6 : 5;
}

void rect(gfx_rectangle &r, int x0, int y0, int x1, int y1)
{
    r.minx = static_cast<int16_t>(x0);
    r.miny = static_cast<int16_t>(y0);
    r.maxx = static_cast<int16_t>(x1);
    r.maxy = static_cast<int16_t>(y1);
}

void nominal(gfx_env *env, const mode &m, int &w, int &h)
{
    if (m.mon >= NBUILTIN) {
        w = 160 << bres(m);
        h = mons31[m.mon - NBUILTIN].rows << ((m.k->flags & K_LACE) ? 1 : 0);
        return;
    }
    w = 320 << m.k->res;
    h = (mon_pal(env, m.mon) ? 256 : 200) << ((m.k->flags & K_LACE) ? 1 : 0);
}

void copy_out(void *buf, const void *src, uint32_t n)
{
    uint8_t *d = static_cast<uint8_t *>(buf);
    const uint8_t *s = static_cast<const uint8_t *>(src);
    while (n--) *d++ = *s++;
}

int ncmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x = static_cast<char>(x + 32);
        if (y >= 'A' && y <= 'Z') y = static_cast<char>(y + 32);
        if (x != y) return 1;
        if (!x) return 0;
    }
}

uint32_t hash_of(const void *key) { return (static_cast<uint32_t>(reinterpret_cast<uintptr_t>(key)) >> 3) % NHASH; }

uint32_t extnode_size(int type)
{
    switch (type) {
    case GD_VIEW_EXTRA_TYPE: return sizeof(gd_viewextra);
    case GD_VIEWPORT_EXTRA_TYPE: return sizeof(gd_vpextra);
    case GD_SPECIAL_MONITOR_TYPE: return sizeof(gd_specialmonitor);
    case GD_MONITOR_SPEC_TYPE: return sizeof(gd_monitorspec);
    default: return 0;
    }
}

/* ---- the tree a driver links its records into ---- */

uint32_t rec_len(const gd_queryheader *h) { return 16 + h->length * 8; }

/* the mode's record that a driver has given data; null for none */
gd_dbnode *db_find(gfx_env *env, uint32_t id)
{
    for (gd_dbnode *m = st(env)->dbroot.child; m; m = m->succ)
        if (m->id == id >> 16)
            for (gd_dbnode *n = m->child; n; n = n->succ)
                if (n->id == id && n->data) return n;
    return nullptr;
}

bool in_modeids(gfx_env *env, const void *h)
{
    const uint32_t *p = static_cast<const uint32_t *>(h), *a = st(env)->modeids;
    return p >= a && p < a + NMODES;
}

/* a handle that is one of the tree's records with data */
bool db_known(gfx_env *env, const void *h)
{
    for (gd_dbnode *m = st(env)->dbroot.child; m; m = m->succ)
        for (gd_dbnode *n = m->child; n; n = n->succ)
            if (n == h && n->data) return true;
    return false;
}

gd_queryheader *db_record(gd_dbnode *n, uint32_t tagid)
{
    for (uint8_t *p = static_cast<uint8_t *>(n->data); p;) {
        gd_queryheader *h = reinterpret_cast<gd_queryheader *>(p);
        if (h->structid == tagid) return h;
        const uint32_t *t = reinterpret_cast<const uint32_t *>(p + rec_len(h));
        p = t[0] == GFX_TAG_MORE ? reinterpret_cast<uint8_t *>(static_cast<uintptr_t>(t[1])) : nullptr;
    }
    return nullptr;
}

/* A monitor's modes share one MonitorInfo, kept on whichever mode it was
 * added to (OS 3.2.3: the driver adds it to the first) */
gd_queryheader *db_record_shared(gd_dbnode *n, uint32_t tagid)
{
    gd_queryheader *h = db_record(n, tagid);
    if (h || tagid != GD_DTAG_MNTR || !n->parent) return h;
    for (gd_dbnode *c = n->parent->child; c && !h; c = c->succ)
        if (c->data) h = db_record(c, tagid);
    return h;
}

/* a driver's DimensionInfo, as GetDisplayInfoData answers it: the record
 * has longwords in ticks, Nominal, MaxOScan and VideoOScan, and each comes
 * out divided by the mode's resolution; the text and standard overscans
 * are the nominal (OS 3.2.3, MonitorProbe) */
uint32_t db_dims(gd_dbnode *n, const gd_queryheader *h, gd_dimensioninfo &out)
{
    const gd_queryheader *dh = db_record(n, GD_DTAG_DISP);
    const int rx = dh ? reinterpret_cast<const gd_displayinfo *>(dh)->resolution.x : 1;
    const int ry = dh ? reinterpret_cast<const gd_displayinfo *>(dh)->resolution.y : 1;
    const uint8_t *p = reinterpret_cast<const uint8_t *>(h) + sizeof *h;
    auto word = [&](int o) { return static_cast<uint16_t>(p[o] << 8 | p[o + 1]); };
    auto lng = [&](int o) { return static_cast<int32_t>(static_cast<uint32_t>(word(o)) << 16 | word(o + 2)); };
    auto box = [&](gfx_rectangle &r, int o) {
        rect(r, lng(o) / (rx ? rx : 1), lng(o + 4) / (ry ? ry : 1), lng(o + 8) / (rx ? rx : 1), lng(o + 12) / (ry ? ry : 1));
    };
    zero(&out, sizeof out);
    out.header = *h;
    out.maxdepth = word(0);
    out.minrasterwidth = word(2);
    out.minrasterheight = word(4);
    out.maxrasterwidth = word(6);
    out.maxrasterheight = word(8);
    box(out.nominal, 10);
    box(out.maxoscan, 26);
    box(out.videooscan, 42);
    out.txtoscan = out.stdoscan = out.nominal;
    return 66;                          /* the fields above */
}

/* a call's mode: its handle, or by ID with none; a handle of ours (a
 * built-in mode's) has no records */
gd_dbnode *db_target(gfx_env *env, void *handle, uint32_t id)
{
    if (handle) return in_modeids(env, handle) ? nullptr : static_cast<gd_dbnode *>(handle);
    return db_find(env, id);
}

void build_tree(gfx_env *env, state *s)
{
    gd_dbnode *prev = nullptr, *m = s->dbmodes;
    for (int i = 0; i < NBUILTIN; i++) {
        gd_dbnode *mon = &s->dbmon[i], *last = nullptr;
        mon->id = monids[i] >> 16;
        mon->parent = &s->dbroot;
        mon->pred = prev;
        if (prev) prev->succ = mon;
        else s->dbroot.child = mon;
        prev = mon;
        for (int j = 0; j < NKEYS; j++, m++) {
            m->id = monids[i] | keys[j].key;
            m->parent = mon;
            m->pred = last;
            if (last) last->succ = m;
            else mon->child = m;
            last = m;
        }
    }
    (void)env;
}

void build_copinit(gfx_env *env, gd_copinit *ci)
{
    auto mv = [](uint16_t *w, int i, uint16_t reg, uint16_t val) {
        w[2 * i] = reg;
        w[2 * i + 1] = val;
    };
    mv(ci->vsync_hblank, 0, GD_NOOP, 0);
    for (int i = 0; i < 6; i++) mv(ci->diagstrt, i, GD_NOOP, 0);
    mv(ci->fm0, 0, GD_NOOP, 0);
    mv(ci->diwstart, 0, GD_DIWSTRT, 0x2C81);
    mv(ci->diwstart, 1, GD_DIWSTOP, 0xF4C1);
    mv(ci->diwstart, 2, GD_DDFSTRT, 0x0038);
    mv(ci->diwstart, 3, GD_DDFSTOP, 0x00D0);
    mv(ci->diwstart, 4, GD_BPLCON0, static_cast<uint16_t>(0x0200 | (base(env)->system_bplcon0 & 0x0102)));
    mv(ci->bplcon2, 0, GD_BPLCON2, 0x0024);
    for (int i = 0; i < 8; i++) mv(ci->sprfix, i, GD_NOOP, 0);
    uint32_t a = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ci->sprstop));
    for (int i = 0; i < 8; i++) {
        mv(ci->sprstrtup, 2 * i, static_cast<uint16_t>(GD_SPR0PTH + 4 * i), static_cast<uint16_t>(a >> 16));
        mv(ci->sprstrtup, 2 * i + 1, static_cast<uint16_t>(GD_SPR0PTH + 4 * i + 2), static_cast<uint16_t>(a));
    }
    mv(ci->wait14, 0, GD_NOOP, 0);
    mv(ci->norm_hblank, 0, GD_NOOP, 0);
    mv(ci->jump, 0, GD_COPJMP2, 0);
    ci->wait_forever[0] = 0xFFFF;
    ci->wait_forever[1] = 0xFFFE;
    mv(ci->wait_forever, 1, GD_NOOP, 0);
    mv(ci->wait_forever, 2, GD_NOOP, 0);
    for (int i = 0; i < 8; i++) ci->sprstop[i] = 0;
}

gd_monitorspec *new_monitor(gfx_env *env, int i, bool pal)
{
    gd_monitorspec *ms = reinterpret_cast<gd_monitorspec *>(gd_GfxNew(env, GD_MONITOR_SPEC_TYPE));
    if (!ms) return nullptr;
    ms->n.node.name = const_cast<char *>(monnames[i]);
    ms->ratioh = ms->ratiov = 16;       /* RATIO_UNITY */
    ms->total_rows = pal ? 312 : 262;   /* STANDARD_PAL_ROWS, STANDARD_NTSC_ROWS */
    ms->total_colorclocks = 226;        /* STANDARD_COLORCLOCKS */
    ms->denisemaxdisplaycolumn = 455;   /* STANDARD_DENISE_MAX */
    ms->deniseminDisplaycolumn = 93;    /* STANDARD_DENISE_MIN */
    ms->beamcon0 = pal ? 0x0020 : 0;    /* DISPLAYPAL */
    ms->min_row = pal ? 29 : 21;        /* MIN_PAL_ROW, MIN_NTSC_ROW */
    ms->xoffset = 9;                    /* STANDARD_XOFFSET */
    ms->yoffset = 0;
    rect(ms->legalview, LEGAL_X0, ms->min_row, LEGAL_X1 - 1, ms->total_rows - 1);
    ms->displaycompatible = 0;          /* MCOMPAT_MIXED */
    return ms;
}

/* GetDisplayInfoData's records of a driver's mode, as OS 3.2.3's (MonitorProbe):
 * answers the record's length, 0 for none */
uint32_t info31(gfx_env *env, const mode &m, gd_monitorspec *ms, uint32_t tagid, void *buf)
{
    const mon31 &d = mons31[m.mon - NBUILTIN];
    const bool dbl = monon[m.mon] == ON_DBL, ff = (m.k->flags & K_LACE) != 0;
    const int r = bres(m), yres = ff ? d.vres : 2 * d.vres;
    int w, h;
    nominal(env, m, w, h);
    switch (tagid) {
    case GD_DTAG_DISP: {
        gd_displayinfo &di = *static_cast<gd_displayinfo *>(buf);
        di.notavailable = static_cast<uint16_t>(avail(env, m));
        di.propertyflags = props31(m);
        di.resolution = { static_cast<int16_t>(88 >> r), static_cast<int16_t>(yres) };
        di.pixelspeed = static_cast<uint16_t>(140 >> r);
        di.numstdsprites = 8;
        di.paletterange = aga(env) ? 0xFFFF : 4096;
        di.spriteresolution = { static_cast<int16_t>(176 >> r), d.vres };
        di.redbits = di.greenbits = di.bluebits = aga(env) ? 8 : 4;
        return sizeof di;
    }
    case GD_DTAG_DIMS: {
        gd_dimensioninfo &dm = *static_cast<gd_dimensioninfo *>(buf);
        dm.maxdepth = static_cast<uint16_t>(maxdepth(env, m));
        dm.minrasterwidth = 16;
        dm.minrasterheight = 1;
        dm.maxrasterwidth = 16368;
        dm.maxrasterheight = 16384;
        rect(dm.nominal, 0, 0, w - 1, h - 1);
        /* the overscan: DblNTSC and DblPAL w/16 pixels and `extra` rows
         * (No Flicker: twice that and one more above) each side, the video
         * overscan 13w/80 to the left; Multiscan w/80 and 9w/80, no rows */
        if (dbl)
            rect(dm.maxoscan, -w / 16, ff ? -2 * d.extra - 1 : -d.extra, w + w / 16 - 1,
                 ff ? 2 * (d.rows + d.extra) : d.rows + d.extra);
        else
            rect(dm.maxoscan, -w / 80, 0, w + w / 80 - 1, h - 1);
        dm.videooscan = dm.maxoscan;
        dm.videooscan.minx = static_cast<int16_t>(-w * (dbl ? 13 : 9) / 80);
        dm.txtoscan = dm.stdoscan = dm.nominal;
        return sizeof dm;
    }
    case GD_DTAG_MNTR: {
        gd_monitorinfo &mi = *static_cast<gd_monitorinfo *>(buf);
        mi.mspc = ms;
        mi.viewposition = mi.defaultviewposition = { d.posx, d.posy };
        mi.viewresolution = { 88, d.vres };
        rect(mi.viewpositionrange, 81, ms->min_row, d.rx1, d.ry1);
        mi.totalrows = ms->total_rows;
        mi.totalcolorclocks = ms->total_colorclocks;
        mi.minrow = ms->min_row;
        mi.compatibility = 0;                               /* MCOMPAT_MIXED */
        mi.mouseticks = { 22, d.vres };
        mi.preferredmodeid = d.pref;
        return sizeof mi;
    }
    case GD_DTAG_NAME: {
        /* named: DblNTSC's and DblPAL's Low and High Res, plain and No
         * Flicker; Multiscan's Extra-Low Res, Low Res and Productivity (No
         * Flicker) and Productivity Dblescan; no other */
        const char *t = nullptr;
        switch (m.k->key) {
        case 0x0000: t = dbl ? "Low Res" : nullptr; break;
        case 0x8000: t = dbl ? "High Res" : nullptr; break;
        case 0x0004: t = dbl ? "Low Res No Flicker" : "Extra-Low Res"; break;
        case 0x8004: t = dbl ? "High Res No Flicker" : "Low Res"; break;
        case 0x8020: t = "Productivity Dblescan"; break;
        case 0x8024: t = "Productivity"; break;
        }
        if (!t) return 0;
        gd_nameinfo &ni = *static_cast<gd_nameinfo *>(buf);
        char *p = reinterpret_cast<char *>(ni.name);
        for (const char *q = d.name; *q;) *p++ = *q++;
        while (*t) *p++ = *t++;
        return sizeof ni;
    }
    }
    return 0;
}

} // namespace

namespace gd {

int scan_of(const gd_monitorspec *ms)
{
    const char *n = ms ? ms->n.node.name : nullptr;
    if (!n) return SCAN_NONE;
    if (!ncmp(n, monnames[3])) return SCAN_MULTI;
    return !ncmp(n, monnames[4]) || !ncmp(n, monnames[5]) ? SCAN_DBL : SCAN_NONE;
}

} // namespace gd

extern "C" {

/* ---- extended nodes -------------------------------------------------------- */

gd_extnode *gd_GfxNew(gfx_env *env, uint32_t type)
{
    uint32_t size = extnode_size(static_cast<int>(type));
    if (!size) return nullptr;
    gd_extnode *n = static_cast<gd_extnode *>(alloc(env, size, GFX_MEMF_PUBLIC | GFX_MEMF_CLEAR));
    if (!n) return nullptr;
    n->node.type = GD_NT_GRAPHICS;
    n->subsystem = GD_SS_GRAPHICS;
    n->subtype = static_cast<uint8_t>(type);
    n->library = static_cast<int32_t>(reinterpret_cast<uintptr_t>(env->base));
    /* a monitor driver calls a MonitorSpec's (OS 3.2 puts a routine in the
     * other types' too: ABI test; nothing has been seen to call those) */
    n->init = type == GD_MONITOR_SPEC_TYPE ? st(env)->monitor_init : nullptr;
    if (type == GD_MONITOR_SPEC_TYPE) {
        gd_monitorspec *ms = reinterpret_cast<gd_monitorspec *>(n);
        list_init(&ms->displayinfodatabase);
        if (env->sem_init) env->sem_init(env->ctx, &ms->displayinfodatabasesemaphore);
    }
    return n;
}

void gd_GfxFree(gfx_env *env, gd_extnode *n)
{
    if (!n) return;
    uint32_t size = extnode_size(n->subtype);
    /* not one of GfxNew's (its type, subsystem and library ours, as GfxNew
     * set them): left alone (ABI test) */
    if (!size || n->node.type != GD_NT_GRAPHICS || n->subsystem != GD_SS_GRAPHICS ||
        n->library != static_cast<int32_t>(reinterpret_cast<uintptr_t>(env->base)))
        return;
    state *s = st(env);
    env->forbid(env->ctx);
    for (int i = 0; i < NHASH; i++)
        for (assoc **pp = &s->hash[i]; *pp;) {
            if ((*pp)->node == n) {
                assoc *a = *pp;
                *pp = a->next;
                release(env, a, sizeof(assoc));
            } else
                pp = &(*pp)->next;
        }
    env->permit(env->ctx);
    release(env, n, size);
}

void gd_GfxAssociate(gfx_env *env, const void *key, gd_extnode *node)
{
    state *s = st(env);
    if (!key) return;
    env->forbid(env->ctx);
    assoc **pp = &s->hash[hash_of(key)];
    for (assoc *a = *pp; a; a = a->next)
        if (a->key == key) {
            a->node = node;
            env->permit(env->ctx);
            return;
        }
    assoc *a = static_cast<assoc *>(alloc(env, sizeof(assoc), GFX_MEMF_PUBLIC));
    if (a) {
        a->key = key;
        a->node = node;
        a->next = *pp;
        *pp = a;
    }
    env->permit(env->ctx);
}

gd_extnode *gd_GfxLookUp(gfx_env *env, const void *key)
{
    state *s = st(env);
    if (!key || !s) return nullptr;
    gd_extnode *n = nullptr;
    env->forbid(env->ctx);              /* as GfxFree and GfxAssociate change the chains */
    for (assoc *a = s->hash[hash_of(key)]; a; a = a->next)
        if (a->key == key) {
            n = a->node;
            break;
        }
    env->permit(env->ctx);
    return n;
}

/* ---- monitors -------------------------------------------------------------- */

gd_monitorspec *gd_OpenMonitor(gfx_env *env, const char *name, uint32_t id)
{
    gfx_base *gb = base(env);
    gd_monitorspec *ms = nullptr;
    env->forbid(env->ctx);
    if (name) {
        for (gfx_node *n = gb->monitorlist.head; n->succ; n = n->succ)
            if (n->name && !ncmp(n->name, name)) {
                ms = reinterpret_cast<gd_monitorspec *>(n);
                break;
            }
    } else {
        uint32_t mon = id == GD_INVALID_ID ? 0 : id & GD_MONITOR_ID_MASK;
        for (int i = 0; i < NMONITORS; i++)
            if (monids[i] == mon) ms = mspec(env, i);
    }
    /* a monitor only a driver's records know: their MonitorInfo says */
    if (!ms && !name && id != GD_INVALID_ID)
        for (gd_dbnode *m = st(env)->dbroot.child; m && !ms; m = m->succ)
            if (m->id == id >> 16)
                for (gd_dbnode *n = m->child; n && !ms; n = n->succ)
                    if (gd_queryheader *h = n->data ? db_record_shared(n, GD_DTAG_MNTR) : nullptr)
                        ms = reinterpret_cast<gd_monitorinfo *>(h)->mspc;
    if (ms) ms->opencount++;
    env->permit(env->ctx);
    return ms;
}

int32_t gd_CloseMonitor(gfx_env *env, gd_monitorspec *ms)
{
    if (!ms) return 0;
    env->forbid(env->ctx);
    int32_t err = ms->opencount ? 0 : 1;    /* FALSE when closed (autodoc) */
    if (ms->opencount) ms->opencount--;
    env->permit(env->ctx);
    return err;
}

/* ---- the display database --------------------------------------------------- */

void *gd_FindDisplayInfo(gfx_env *env, uint32_t id)
{
    state *s = st(env);
    mode m;
    if (gd_dbnode *n = db_find(env, id)) return n;
    for (int i = 0; i < NMODES; i++)
        if (s->modeids[i] == id) return find_mode(env, id, m) ? &s->modeids[i] : nullptr;
    return nullptr;
}

uint32_t gd_NextDisplayInfo(gfx_env *env, uint32_t id)
{
    state *s = st(env);
    mode m;
    uint32_t best = GD_INVALID_ID;
    /* the next one above id, in or not in the database (ABI test); modeids
     * is in order, a driver's modes there while it is */
    for (int i = 0; i < NMODES; i++)
        if ((id == GD_INVALID_ID || s->modeids[i] > id) && find_mode(env, s->modeids[i], m)) {
            best = s->modeids[i];
            break;
        }
    /* and the records drivers have linked in, whose order is theirs */
    for (gd_dbnode *mon = s->dbroot.child; mon; mon = mon->succ)
        for (gd_dbnode *n = mon->child; n; n = n->succ)
            if (n->data && (id == GD_INVALID_ID || n->id > id) && n->id < best) best = n->id;
    return best;
}

uint32_t gd_GetDisplayInfoData(gfx_env *env, void *handle, void *buf, uint32_t size, uint32_t tagid, uint32_t id)
{
    state *s = st(env);
    gd_dbnode *dn = nullptr;
    if (handle) {
        uint32_t *h = static_cast<uint32_t *>(handle);
        if (h >= s->modeids && h < s->modeids + NMODES) id = *h;
        else if (db_known(env, handle)) dn = static_cast<gd_dbnode *>(handle);
        else return 0;
    }
    if (!dn) dn = db_find(env, id);
    if (dn) {
        const gd_queryheader *h = buf ? db_record_shared(dn, tagid) : nullptr;
        if (!h) return 0;
        uint32_t len = rec_len(h);
        const void *from = h;
        gd_dimensioninfo dims;
        if (tagid == GD_DTAG_DIMS) {
            len = db_dims(dn, h, dims);
            from = &dims;
        }
        if (size > len) size = len;
        copy_out(buf, from, size);
        return size;
    }
    mode m;
    if (!buf || !find_mode(env, id, m)) return 0;
    const bool pal = mon_pal(env, m.mon), lace = (m.k->flags & K_LACE) != 0;
    const int xm = 1 << m.k->res, ym = lace ? 2 : 1;
    gd_monitorspec *ms = mspec(env, m.mon);
    union {
        gd_displayinfo di;
        gd_dimensioninfo dims;
        gd_monitorinfo mi;
        gd_nameinfo ni;
    } u;
    zero(&u, sizeof u);
    uint32_t len;
    if (m.mon >= NBUILTIN) {
        len = info31(env, m, ms, tagid, &u);
        if (!len) return 0;
    } else
    switch (tagid) {
    case GD_DTAG_DISP:
        len = sizeof u.di;
        u.di.notavailable = static_cast<uint16_t>(avail(env, m));
        u.di.propertyflags = props(env, m);
        u.di.resolution = { static_cast<int16_t>(44 / xm), static_cast<int16_t>(44 / ym) };
        u.di.pixelspeed = static_cast<uint16_t>(140 / xm);
        u.di.numstdsprites = 8;
        u.di.paletterange = aga(env) ? 0xFFFF : 4096;      /* ABI test */
        u.di.spriteresolution = { 44, static_cast<int16_t>(44 / ym) };
        u.di.redbits = u.di.greenbits = u.di.bluebits = aga(env) ? 8 : 4;
        break;
    case GD_DTAG_DIMS: {
        len = sizeof u.dims;
        int w, h;
        nominal(env, m, w, h);
        int rows = ms ? ms->total_rows : (pal ? 312 : 262), minrow = ms ? ms->min_row : (pal ? 29 : 21);
        bool big = (base(env)->chiprevbits0 & GFX_GFXF_HR_AGNUS) != 0;
        u.dims.maxdepth = static_cast<uint16_t>(maxdepth(env, m));
        u.dims.minrasterwidth = 16;
        u.dims.minrasterheight = 1;
        u.dims.maxrasterwidth = big ? 16368 : 1008;
        u.dims.maxrasterheight = big ? 16384 : 1008;
        rect(u.dims.nominal, 0, 0, w - 1, h - 1);
        rect(u.dims.maxoscan, (LEGAL_X0 - VIEW_X) * xm, (minrow - VIEW_Y) * ym, (LEGAL_X1 - VIEW_X) * xm - 1,
             (rows - VIEW_Y) * ym - 1);
        u.dims.videooscan = u.dims.maxoscan;
        u.dims.txtoscan = u.dims.nominal;
        rect(u.dims.stdoscan, -16 * xm, 0, w - 1 + 16 * xm, h - 1);
        break;
    }
    case GD_DTAG_MNTR:
        len = sizeof u.mi;
        u.mi.mspc = ms;
        u.mi.viewposition = { VIEW_X, VIEW_Y };
        u.mi.viewresolution = { 44, 44 };                   /* ABI test */
        rect(u.mi.viewpositionrange, LEGAL_X0, ms ? ms->min_row : 21, LEGAL_X1 - 1, ms ? ms->total_rows - 1 : 261);
        u.mi.totalrows = ms ? ms->total_rows : 262;
        u.mi.totalcolorclocks = ms ? ms->total_colorclocks : 226;
        u.mi.minrow = ms ? ms->min_row : 21;
        u.mi.compatibility = 0;                             /* MCOMPAT_MIXED */
        u.mi.mouseticks = { static_cast<int16_t>(44 / xm), static_cast<int16_t>(44 / ym) };
        u.mi.defaultviewposition = { VIEW_X, VIEW_Y };
        u.mi.preferredmodeid = monids[m.mon] | GD_HIRES_KEY;
        break;
    case GD_DTAG_NAME: {
        if (m.mon == 0) return 0;       /* the default monitor's modes have no names (ABI test) */
        len = sizeof u.ni;
        char *p = reinterpret_cast<char *>(u.ni.name), *e = p + sizeof u.ni.name - 1;
        auto add = [&](const char *t) { while (*t && p < e) *p++ = *t++; };
        add(m.mon == 1 ? "NTSC:" : "PAL:");
        uint8_t f = m.k->flags;
        if ((f & K_HAM) && m.k->res == 0) add("HAM");
        else if ((f & K_EHB) && m.k->res == 0) add("Extra-HalfBrite");
        else {
            add(resnames[m.k->res]);
            if (f & K_HAM) add(" HAM");
            if (f & K_EHB) add(" EHB");
        }
        if (f & K_LACE) add(" Laced");
        if (f & K_DPF) add((f & K_DPF2) ? " DualPF2" : " DualPF");
        break;
    }
    default:
        return 0;                       /* DTAG_VEC and others: none (ABI test) */
    }
    u.di.header.structid = tagid;
    u.di.header.displayid = id;
    u.di.header.skipid = GFX_TAG_SKIP;
    u.di.header.length = (len + 7) / 8;
    if (size > len) size = len;
    copy_out(buf, &u, size);
    return size;
}

uint32_t gd_GetVPModeID(gfx_env *env, const gd_viewport *vp)
{
    (void)env;
    if (!vp) return GD_INVALID_ID;
    const gfx_colormap *cm = vp->colormap;
    if (cm && cm->type >= GFX_COLORMAP_TYPE_V36 && cm->vpmodeid != GD_INVALID_ID) return cm->vpmodeid;
    /* an old ViewPort: its Modes on the default monitor (ABI test) */
    return vp->modes & (GD_LACE | GD_SUPERHIRES | GD_PFBA | GD_EXTRA_HALFBRITE | GD_DUALPF | GD_HAM | GD_HIRES);
}

uint32_t gd_ModeNotAvailable(gfx_env *env, uint32_t id)
{
    mode m;
    if (gd_dbnode *n = db_find(env, id))
        if (const gd_queryheader *h = db_record(n, GD_DTAG_DISP))
            return reinterpret_cast<const gd_displayinfo *>(h)->notavailable;
    if (find_mode(env, id, m)) return avail(env, m);
    for (int i = 0; i < NMONITORS; i++)
        if ((id & GD_MONITOR_ID_MASK) == monids[i] && mspec(env, i)) return GD_DI_AVAIL_NOCHIPS;
    return GD_DI_AVAIL_NOMONITOR;       /* ABI test */
}

uint32_t gd_BestModeIDA(gfx_env *env, const gfx_tagitem *tags)
{
    const uint32_t special = GD_DIPF_IS_DUALPF | GD_DIPF_IS_PF2PRI | GD_DIPF_IS_HAM | GD_DIPF_IS_EXTRAHALFBRITE;
    uint32_t must = 0, mustnot = special, source = GD_INVALID_ID, monitor = GD_INVALID_ID;
    int nomw = 640, nomh = 200, desw = 0, desh = 0, depth = 1, rb = 0, gbits = 0, bb = 0;
    bool f;
    /* a ViewPort and a source mode first, then the explicit tags over them */
    gd_viewport *vp = reinterpret_cast<gd_viewport *>(static_cast<uintptr_t>(tag_data(tags, GD_BIDTAG_ViewPort, 0)));
    if (vp) {
        source = gd_GetVPModeID(env, vp);
        nomw = vp->dwidth;
        nomh = vp->dheight;
        if (vp->rasinfo && vp->rasinfo->bitmap) depth = vp->rasinfo->bitmap->depth;
    }
    source = tag_data(tags, GD_BIDTAG_SourceID, source);
    mode sm;
    bool hassrc = source != GD_INVALID_ID && find_mode(env, source, sm);
    if (hassrc) {
        uint32_t sp = props(env, sm) & special;
        must |= sp;
        mustnot &= ~sp;
        if (!vp) nominal(env, sm, nomw, nomh);
    }
    must = tag_data(tags, GD_BIDTAG_DIPFMustHave, must);
    mustnot = tag_data(tags, GD_BIDTAG_DIPFMustNotHave, mustnot & ~must);   /* what is a must is not barred */
    nomw = static_cast<int>(tag_data(tags, GD_BIDTAG_NominalWidth, static_cast<uint32_t>(nomw)));
    nomh = static_cast<int>(tag_data(tags, GD_BIDTAG_NominalHeight, static_cast<uint32_t>(nomh)));
    desw = static_cast<int>(tag_data(tags, GD_BIDTAG_DesiredWidth, static_cast<uint32_t>(nomw)));
    desh = static_cast<int>(tag_data(tags, GD_BIDTAG_DesiredHeight, static_cast<uint32_t>(nomh)));
    depth = static_cast<int>(tag_data(tags, GD_BIDTAG_Depth, static_cast<uint32_t>(depth)));
    monitor = tag_data(tags, GD_BIDTAG_MonitorID, monitor, &f);
    rb = static_cast<int>(tag_data(tags, GD_BIDTAG_RedBits, 0));
    gbits = static_cast<int>(tag_data(tags, GD_BIDTAG_GreenBits, 0));
    bb = static_cast<int>(tag_data(tags, GD_BIDTAG_BlueBits, 0));
    if (nomw <= 0) nomw = 640;
    if (nomh <= 0) nomh = 200;
    const int bits = aga(env) ? 8 : 4;
    if (rb > bits || gbits > bits || bb > bits) return GD_INVALID_ID;
    uint32_t best = GD_INVALID_ID;
    uint32_t bestscore[4] = { ~0u, ~0u, ~0u, ~0u };
    for (int i = 0; i < NMODES; i++) {
        mode m;
        if (!find_mode(env, st(env)->modeids[i], m)) continue;
        if (monitor != GD_INVALID_ID && (m.id & GD_MONITOR_ID_MASK) != (monitor & GD_MONITOR_ID_MASK)) continue;
        if (avail(env, m)) continue;
        uint32_t p = props(env, m);
        if ((p & must) != must || (p & mustnot)) continue;
        if (maxdepth(env, m) < depth) continue;
        int w, h;
        nominal(env, m, w, h);
        /* fits the desired size, the nominal aspect, the size, the source's monitor */
        uint32_t fits = (w >= desw && h >= desh) ? 0 : 1;
        int cross = w * nomh - h * nomw;
        uint32_t aspect = static_cast<uint32_t>(cross < 0 ? -cross : cross) / static_cast<uint32_t>((h * nomh) / 64 + 1);
        int dw = w - desw, dh = h - desh;
        uint32_t dist = static_cast<uint32_t>((dw < 0 ? -dw : dw) + (dh < 0 ? -dh : dh));
        uint32_t mon = hassrc && (m.id & GD_MONITOR_ID_MASK) != (sm.id & GD_MONITOR_ID_MASK) ? 1 : 0;
        uint32_t score[4] = { fits, aspect, dist, mon };
        bool better = false;
        for (int k = 0; k < 4; k++)
            if (score[k] != bestscore[k]) {
                better = score[k] < bestscore[k];
                break;
            }
        if (better) {
            best = m.id;
            for (int k = 0; k < 4; k++) bestscore[k] = score[k];
        }
    }
    return best;
}

uint32_t gd_CoerceMode(gfx_env *env, gd_viewport *vp, uint32_t monitorid, uint32_t flags)
{
    uint32_t src = gd_GetVPModeID(env, vp);
    mode m;
    if (src == GD_INVALID_ID) return GD_INVALID_ID;
    uint32_t key = src & ~GD_MONITOR_ID_MASK;
    uint32_t mon = monitorid & GD_MONITOR_ID_MASK;
    uint32_t id;
    /* AVOID_FLICKER: the non-interlaced modes first (the autodoc's flag) */
    for (int pass = flags & GD_AVOID_FLICKER && key & GD_LACE ? 0 : 1; pass < 2; pass++) {
        uint32_t k = pass ? key : key & ~uint32_t(GD_LACE);
        id = mon | k;
        if (find_mode(env, id, m) && !avail(env, m)) return id;
        if (flags & GD_PRESERVE_COLORS) continue;
        /* the same resolution without HAM, EHB or dual playfield (ours) */
        id = mon | (k & (GD_HIRES | GD_SUPERHIRES | GD_LACE));
        if (find_mode(env, id, m) && !avail(env, m)) return id;
    }
    return GD_INVALID_ID;
}

/* ---- VideoControl ----------------------------------------------------------- */

uint32_t gd_VideoControl(gfx_env *env, gfx_colormap *cm, gfx_tagitem *tags)
{
    if (!cm) return 1;
    uint32_t err = 0;
    bool immediate = false;
    gd_viewport *vp = static_cast<gd_viewport *>(cm->vp);
    const gfx_tagitem *s = tags;
    auto flag = [&](uint8_t &field, uint8_t bit, bool on) {
        field = static_cast<uint8_t>(on ? field | bit : field & ~bit);
    };
    auto getflag = [](gfx_tagitem *t, bool on, uint32_t set, uint32_t clr) { t->tag = on ? set : clr; };
    while (gfx_tagitem *t = next_tag(&s)) {
        uint32_t d = t->data;
        switch (t->tag) {
        case 0x80000000u: flag(cm->flags, GD_COLORMAP_TRANSPARENCY, false); break;   /* CHROMAKEY_CLR */
        case 0x80000001u: flag(cm->flags, GD_COLORMAP_TRANSPARENCY, true); break;    /* CHROMAKEY_SET */
        case 0x80000002u: flag(cm->flags, GD_COLORPLANE_TRANSPARENCY, false); break; /* BITPLANEKEY_CLR */
        case 0x80000003u: flag(cm->flags, GD_COLORPLANE_TRANSPARENCY, true); break;
        case 0x80000004u: flag(cm->flags, GD_BORDER_BLANKING, false); break;         /* BORDERBLANK_CLR */
        case 0x80000005u: flag(cm->flags, GD_BORDER_BLANKING, true); break;
        case 0x80000006u: flag(cm->flags, GD_BORDER_NOTRANSPARENCY, false); break;   /* BORDERNOTRANS_CLR */
        case 0x80000007u: flag(cm->flags, GD_BORDER_NOTRANSPARENCY, true); break;
        case 0x80000008u:                                                            /* CHROMA_PEN_CLR */
        case 0x80000009u:                                                            /* CHROMA_PEN_SET */
            if (d < cm->count && cm->colortable) {
                if (t->tag & 1) cm->colortable[d] |= 0x8000;
                else cm->colortable[d] &= 0x7FFF;
            }
            break;
        case 0x8000000Au: cm->transparencyplane = static_cast<uint8_t>(d); break;    /* CHROMA_PLANE_SET */
        case 0x8000000Bu: cm->vp = vp = reinterpret_cast<gd_viewport *>(static_cast<uintptr_t>(d)); break; /* ATTACH_CM_SET */
        case 0x8000000Cu:                                                            /* NEXTBUF_CM */
            s = reinterpret_cast<const gfx_tagitem *>(static_cast<uintptr_t>(d));
            break;
        case 0x8000000Du: flag(cm->flags, GD_VIDEOCONTROL_BATCH, false); break;      /* BATCH_CM_CLR */
        case 0x8000000Eu: flag(cm->flags, GD_VIDEOCONTROL_BATCH, true); break;
        case 0x8000000Fu: t->data = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(cm->normaldisplayinfo)); break;
        case 0x80000010u: cm->normaldisplayinfo = reinterpret_cast<void *>(static_cast<uintptr_t>(d)); break;
        case 0x80000011u: t->data = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(cm->coercedisplayinfo)); break;
        case 0x80000012u: cm->coercedisplayinfo = reinterpret_cast<void *>(static_cast<uintptr_t>(d)); break;
        case 0x80000013u: t->data = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(cm->vpe)); break;
        case 0x80000014u: cm->vpe = reinterpret_cast<void *>(static_cast<uintptr_t>(d)); break;
        case 0x80000015u: getflag(t, cm->flags & GD_COLORMAP_TRANSPARENCY, 0x80000001u, 0x80000000u); break;
        case 0x80000016u: getflag(t, cm->flags & GD_COLORPLANE_TRANSPARENCY, 0x80000003u, 0x80000002u); break;
        case 0x80000017u: getflag(t, cm->flags & GD_BORDER_BLANKING, 0x80000005u, 0x80000004u); break;
        case 0x80000018u: getflag(t, cm->flags & GD_BORDER_NOTRANSPARENCY, 0x80000007u, 0x80000006u); break;
        case 0x80000019u: t->data = cm->transparencyplane; break;                    /* CHROMA_PLANE_GET */
        case 0x8000001Au: t->data = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(cm->vp)); break;
        case 0x8000001Bu: getflag(t, cm->flags & GD_VIDEOCONTROL_BATCH, 0x8000000Eu, 0x8000000Du); break;
        case 0x8000001Cu: t->data = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(cm->batch_items)); break;
        case 0x8000001Du:                                                            /* BATCH_ITEMS_SET */
        case 0x8000001Eu:                                                            /* BATCH_ITEMS_ADD: chained by the caller */
            cm->batch_items = reinterpret_cast<gfx_tagitem *>(static_cast<uintptr_t>(d));
            break;
        case 0x8000001Fu: t->data = cm->vpmodeid; break;                             /* VPMODEID_GET */
        case 0x80000020u: cm->vpmodeid = d; break;                                   /* VPMODEID_SET */
        case 0x80000021u: cm->vpmodeid = GD_INVALID_ID; break;                       /* VPMODEID_CLR */
        case 0x80000022u: getflag(t, cm->flags & GD_USER_COPPER_CLIP, 0x80000023u, 0x80000024u); break;
        case 0x80000023u: flag(cm->flags, GD_USER_COPPER_CLIP, true); break;
        case 0x80000024u: flag(cm->flags, GD_USER_COPPER_CLIP, false); break;
        case 0x80000025u: t->data = cm->bp_0_base; break;                            /* PF1_BASE_GET */
        case 0x80000026u: t->data = cm->bp_1_base; break;
        case 0x80000027u: t->data = cm->spritebase_even; break;
        case 0x80000028u: t->data = cm->spritebase_odd; break;
        case 0x80000029u: cm->bp_0_base = static_cast<uint16_t>(d); break;           /* PF1_BASE_SET */
        case 0x8000002Au: cm->bp_1_base = static_cast<uint16_t>(d); break;
        case 0x8000002Bu: cm->spritebase_even = static_cast<uint16_t>(d); break;
        case 0x8000002Cu: cm->spritebase_odd = static_cast<uint16_t>(d); break;
        case 0x8000002Du: getflag(t, cm->flags & GD_BORDERSPRITES, 0x8000002Eu, 0x8000002Fu); break;
        case 0x8000002Eu: flag(cm->flags, GD_BORDERSPRITES, true); break;
        case 0x8000002Fu: flag(cm->flags, GD_BORDERSPRITES, false); break;
        case 0x80000030u: cm->spriteresolution = static_cast<uint8_t>(d); break;    /* SPRITERESN_SET */
        case 0x80000031u: t->data = static_cast<uint32_t>(static_cast<int8_t>(cm->spriteresolution)); break;
        case 0x80000032u:                                                            /* PF1_TO_SPRITEPRI_SET */
            if (vp) vp->spritepriorities = static_cast<uint8_t>((vp->spritepriorities & ~7) | (d & 7));
            break;
        case 0x80000033u: t->data = vp ? vp->spritepriorities & 7u : 0; break;
        case 0x80000034u:
            if (vp) vp->spritepriorities = static_cast<uint8_t>((vp->spritepriorities & ~0x38) | ((d & 7) << 3));
            break;
        case 0x80000035u: t->data = vp ? (vp->spritepriorities >> 3) & 7u : 0; break;
        case 0x80000036u: immediate = true; break;                                   /* IMMEDIATE */
        case 0x80000037u: cm->auxflags |= GD_CMAF_FULLPALETTE; break;
        case 0x80000038u: getflag(t, cm->auxflags & GD_CMAF_FULLPALETTE, 0x80000037u, 0x80000039u); break;
        case 0x80000039u: cm->auxflags &= static_cast<uint8_t>(~GD_CMAF_FULLPALETTE); break;
        case 0x8000003Au: cm->spriteresdefault = static_cast<uint8_t>(d); break;     /* DEFSPRITERESN_SET */
        case 0x8000003Bu: t->data = static_cast<uint32_t>(static_cast<int8_t>(cm->spriteresdefault)); break;
        case 0x80000080u: flag(cm->auxflags, GD_CMAF_NO_INTERMED_UPDATE, !d); break; /* VC_IntermediateCLUpdate */
        case 0x80000081u: t->data = !(cm->auxflags & GD_CMAF_NO_INTERMED_UPDATE); break;
        case 0x80000082u: flag(cm->auxflags, GD_CMAF_NO_COLOR_LOAD, d != 0); break;  /* VC_NoColorPaletteLoad */
        case 0x80000083u: t->data = (cm->auxflags & GD_CMAF_NO_COLOR_LOAD) != 0; break;
        case 0x80000084u: flag(cm->auxflags, GD_CMAF_DUALPF_DISABLE, d != 0); break; /* VC_DUALPF_Disable */
        case 0x80000085u: t->data = (cm->auxflags & GD_CMAF_DUALPF_DISABLE) != 0; break;
        default: err = 1; break;        /* an unknown tag (ABI test) */
        }
    }
    if (immediate && vp && vp->dspins) patch_vp(env, vp, false);
    return err;
}

/* ---- a monitor driver's calls ---------------------------------------------- */

void gd_set_monitor_init(gfx_env *env, void *routine)
{
    state *s = st(env);
    s->monitor_init = routine;
    for (int i = 0; i < NBUILTIN; i++) s->monitors[i]->n.init = routine;
}

/* As OS 3.2.3's (tests/DriverCallLog.c): the driver's own flags
 * (MSF_REQUEST_NTSC 1, PAL 2, SPECIAL 4, A2024 8) and the standard timing,
 * and a SpecialMonitor if asked for; the driver overwrites most of it */
uint32_t gd_monitor_init(gfx_env *env, gd_monitorspec *ms, uint32_t flags)
{
    const bool pal = (flags & 2) != 0 || (!(flags & 1) && (base(env)->displayflags & GFX_PAL));
    ms->flags = static_cast<uint16_t>(flags);
    ms->ratioh = ms->ratiov = 16;
    ms->total_rows = pal ? 312 : 262;
    ms->total_colorclocks = 226;
    ms->denisemaxdisplaycolumn = 455;
    ms->deniseminDisplaycolumn = 93;
    ms->beamcon0 = (flags & 4) ? 0x1BAA : (pal ? 0x0020 : 0);
    ms->min_row = pal ? 29 : 21;
    ms->xoffset = 9;
    ms->yoffset = 0;
    rect(ms->legalview, VIEW_X, VIEW_Y, VIEW_X, VIEW_Y);
    if ((flags & 4) && !ms->special) ms->special = reinterpret_cast<gd_specialmonitor *>(gd_GfxNew(env, GD_SPECIAL_MONITOR_TYPE));
    return flags;
}

/* -744: a driver's data record joins its mode's, a copy of buf */
uint32_t gd_AddDisplayInfoData(gfx_env *env, void *handle, const void *buf, uint32_t size, uint32_t tagid, uint32_t id)
{
    (void)tagid;                        /* buf says */
    gd_dbnode *n = db_target(env, handle, id);
    if (!n || !buf || size < sizeof(gd_queryheader)) return 0;
    const uint32_t len = rec_len(static_cast<const gd_queryheader *>(buf));
    if (len > size) return 0;
    uint8_t *r = static_cast<uint8_t *>(alloc(env, len + 8, GFX_MEMF_PUBLIC | GFX_MEMF_CLEAR));
    if (!r) return 0;
    copy_out(r, buf, len);
    uint32_t *t = reinterpret_cast<uint32_t *>(r + len);
    env->forbid(env->ctx);
    if (n->data) {
        t[0] = GFX_TAG_MORE;
        t[1] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(n->data));
    }
    n->more = GFX_TAG_MORE;
    n->data = r;
    env->permit(env->ctx);
    return size;
}

/* -750: the driver changes a record, a monitor's shared MonitorInfo too
 * (which it writes whole, Mspc and all, yet OS 3.2.3 answers 20 fewer) */
uint32_t gd_SetDisplayInfoData(gfx_env *env, void *handle, const void *buf, uint32_t size, uint32_t tagid, uint32_t id)
{
    gd_dbnode *n = db_target(env, handle, id);
    if (!n || !n->data || !buf) return 0;
    gd_queryheader *h = db_record_shared(n, tagid);
    if (!h) return 0;
    const uint32_t len = rec_len(h);
    if (size > len) size = len;
    copy_out(h, buf, size);
    return tagid == GD_DTAG_MNTR && size > 20 ? size - 20 : size;
}

/* ---- the display's start ---------------------------------------------------- */

int gd_init(gfx_env *env, int own_hw)
{
    gfx_base *gb = base(env);
    state *s = static_cast<state *>(alloc(env, sizeof(state), GFX_MEMF_PUBLIC | GFX_MEMF_CLEAR));
    if (!s) return 0;
    gfx_private_of(gb)->area[GFX_AREA_DISPLAY] = s;
    s->copinit = static_cast<gd_copinit *>(alloc(env, sizeof(gd_copinit), GFX_MEMF_CHIP | GFX_MEMF_CLEAR));
    if (!s->copinit) {
        gd_exit(env);
        return 0;
    }
    s->spritewords = 1;
    s->hw = own_hw != 0;
    gb->copinit = s->copinit;
    gb->simplesprites = reinterpret_cast<void **>(s->sprites);
    gb->hash_table = reinterpret_cast<int16_t *>(s->hash);
    s->dbmodes = static_cast<gd_dbnode *>(alloc(env, NBUILTIN * NKEYS * sizeof(gd_dbnode), GFX_MEMF_PUBLIC | GFX_MEMF_CLEAR));
    if (!s->dbmodes) {
        gd_exit(env);
        return 0;
    }
    gb->displayinfodatabase = &s->dbroot;
    for (int i = 0; i < NSEMS; i++)
        if (env->sem_init) env->sem_init(env->ctx, &s->sems[i]);
    gb->lastchancememory = &s->sems[SEM_LASTCHANCE];
    gb->actiewcprsemaphore = &s->sems[SEM_ACTIVIEWCPR];
    gb->monitorlistsemaphore = &s->sems[SEM_MONITORLIST];
    gb->hashtablesemaphore = &s->sems[SEM_HASHTABLE];
    int n = 0;
    for (int i = 0; i < NMONITORS; i++)
        for (int j = 0; j < NKEYS; j++)
            if (keys[j].on & monon[i]) s->modeids[n++] = monids[i] | keys[j].key;
    build_tree(env, s);

    /* the chips: Agnus says PAL or NTSC */
    unsigned agnus = (env->chip_read(env->ctx, GD_VPOSR) >> 8) & 0x7F;
    bool pal = (agnus & 0x10) || agnus == 0x23;     /* ABI test: Alice's PAL ID */
    gb->displayflags = static_cast<uint16_t>((gb->displayflags & GFX_GENLOC) | (pal ? GFX_PAL | GFX_REALLY_PAL : GFX_NTSC));
    build_copinit(env, s->copinit);
    gd_SetChipRev(env, 0xFFFFFFFFu);    /* SETCHIPREV_BEST */

    for (int i = 0; i < NBUILTIN; i++) {
        s->monitors[i] = new_monitor(env, i, i == 2 || (i == 0 && pal));
        if (!s->monitors[i]) {
            gd_exit(env);
            return 0;
        }
        add_tail(&gb->monitorlist, &s->monitors[i]->n.node);
    }
    gb->default_monitor = s->monitors[0];
    gb->natural_monitor = s->monitors[pal ? 2 : 1];
    gb->current_monitor = s->monitors[0];
    gb->monitor_id = 0;
    gb->current_tot_rows = s->monitors[0]->total_rows;
    gb->current_tot_cclks = s->monitors[0]->total_colorclocks;
    /* ABI test: OS 3.2's values for these */
    gb->normaldisplayrows = pal ? 256 : 200;
    gb->normaldisplaycolumns = 640;
    gb->maxdisplayrow = static_cast<uint16_t>(s->monitors[0]->total_rows - 1);
    gb->maxdisplaycolumn = 455;
    gb->mindisplaycolumn = 93;
    gb->normaldpmx = 1280;
    gb->normaldpmy = pal ? 1280 : 1000;
    gb->microsperline = pal ? 64 * 256 : 63 * 256 + 128;
    gb->defaultspritewidth = 16;
    set_fmode(env);
    for (int i = 0; i < 8; i++) set_sprite_ptr(env, i, nullptr);

    /* the copper on copinit, then DMA: copper, bitplanes, sprites */
    gd_LoadView(env, nullptr);
    put(env, GD_COPJMP1, 0);
    put(env, 0x096, 0x83A0);   /* DMACON: SET, DMAEN, BPLEN, COPEN, SPREN */
    return 1;
}

void gd_exit(gfx_env *env)
{
    gfx_base *gb = base(env);
    state *s = st(env);
    if (!s) return;
    for (int i = 0; i < NBUILTIN; i++)
        if (s->monitors[i]) {
            remove(&s->monitors[i]->n.node);
            gd_GfxFree(env, &s->monitors[i]->n);
        }
    for (int i = 0; i < NHASH; i++)
        while (assoc *a = s->hash[i]) {
            s->hash[i] = a->next;
            release(env, a, sizeof(assoc));
        }
    /* the data records -744 allocated; a driver's own nodes are its */
    for (gd_dbnode *m = s->dbroot.child; m; m = m->succ)
        for (gd_dbnode *n = m->child; n; n = n->succ)
            for (uint8_t *p = static_cast<uint8_t *>(n->data); p;) {
                const uint32_t len = rec_len(reinterpret_cast<gd_queryheader *>(p));
                const uint32_t *t = reinterpret_cast<const uint32_t *>(p + len);
                uint8_t *next = t[0] == GFX_TAG_MORE ? reinterpret_cast<uint8_t *>(static_cast<uintptr_t>(t[1])) : nullptr;
                release(env, p, len + 8);
                p = next;
            }
    release(env, s->dbmodes, NBUILTIN * NKEYS * sizeof(gd_dbnode));
    if (gb->copinit == s->copinit) gb->copinit = nullptr;
    gb->simplesprites = nullptr;
    gb->hash_table = nullptr;
    gb->displayinfodatabase = nullptr;
    gb->lastchancememory = gb->actiewcprsemaphore = gb->monitorlistsemaphore = gb->hashtablesemaphore = nullptr;
    gb->default_monitor = gb->natural_monitor = gb->current_monitor = nullptr;
    release(env, s->copinit, sizeof(gd_copinit));
    gfx_private_of(gb)->area[GFX_AREA_DISPLAY] = nullptr;
    release(env, s, sizeof(state));
}

} // extern "C"
