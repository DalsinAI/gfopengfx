/* SPDX-License-Identifier: MIT */
/* MonitorProbe: what the running graphics.library's monitors and display
 * database say, for GFOpenGfx's DblPAL, DblNTSC and Multiscan entries.
 * Copyright (c) 2026 AmiCradle Team. MIT.
 *
 *   MonitorProbe
 * prints every MonitorSpec on GfxBase->MonitorList (its fields, and its
 * SpecialMonitor's flags and blank and sync intervals), then every mode in
 * the display database: NotAvailable, PropertyFlags, Resolution,
 * PixelSpeed, MaxDepth, Nominal, MaxOScan, VideoOScan, TxtOScan,
 * StdOScan, MonitorInfo's TotalRows, TotalColorClocks, MinRow,
 * ViewPosition, ViewPositionRange, ViewResolution, MouseTicks,
 * DefaultViewPosition, PreferredModeID and Compatibility, and its name.
 *
 *   MonitorProbe ID=000A1000 [DEPTH=2]
 * opens a screen of that mode behind, and prints its View's DxOffset and
 * DyOffset, its ViewPort's place and size, BEAMCON0 as GfxBase keeps it
 * (system_bplcon0, the MonitorSpec's BeamCon0), and every MOVE of the
 * View's long frame copper list as "reg=value", then closes it.
 * Run the monitor drivers (DEVS:Monitors) first. */
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfxbase.h>
#include <graphics/monitor.h>
#include <graphics/displayinfo.h>
#include <graphics/modeid.h>
#include <graphics/view.h>
#include <graphics/videocontrol.h>
#include <dos/dos.h>
#include <dos/rdargs.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/dos.h>
#include <stdlib.h>

extern struct GfxBase *GfxBase;

static void rect(const char *n, struct Rectangle *r)
{
    Printf(" %s %ld,%ld-%ld,%ld", (LONG)n, (LONG)r->MinX, (LONG)r->MinY, (LONG)r->MaxX, (LONG)r->MaxY);
}

static void monitors(void)
{
    struct Node *n;
    Forbid();
    for (n = GfxBase->MonitorList.lh_Head; n->ln_Succ; n = n->ln_Succ) {
        struct MonitorSpec *ms = (struct MonitorSpec *)n;
        Printf("MONITOR %s flags %04lx ratio %ld,%ld rows %ld cclks %ld denisemax %ld denisemin %ld beamcon0 %04lx"
               " minrow %ld offset %ld,%ld compat %ld",
               (LONG)(n->ln_Name ? n->ln_Name : "?"), (LONG)ms->ms_Flags, ms->ratioh, ms->ratiov,
               (LONG)ms->total_rows, (LONG)ms->total_colorclocks, (LONG)ms->DeniseMaxDisplayColumn,
               (LONG)ms->DeniseMinDisplayColumn, (LONG)ms->BeamCon0, (LONG)ms->min_row, (LONG)ms->ms_xoffset,
               (LONG)ms->ms_yoffset, (LONG)ms->DisplayCompatible);
        rect("legal", &ms->ms_LegalView);
        Printf("\n");
        if (ms->ms_Special) {
            struct SpecialMonitor *sm = ms->ms_Special;
            Printf("  SPECIAL flags %04lx hblank %04lx-%04lx vblank %04lx-%04lx hsync %04lx-%04lx vsync %04lx-%04lx\n",
                   (LONG)sm->spm_Flags, (LONG)sm->hblank.asi_Start, (LONG)sm->hblank.asi_Stop,
                   (LONG)sm->vblank.asi_Start, (LONG)sm->vblank.asi_Stop, (LONG)sm->hsync.asi_Start,
                   (LONG)sm->hsync.asi_Stop, (LONG)sm->vsync.asi_Start, (LONG)sm->vsync.asi_Stop);
        }
    }
    Permit();
}

static void modes(void)
{
    ULONG id = INVALID_ID;
    while ((id = NextDisplayInfo(id)) != INVALID_ID) {
        struct DisplayInfo di;
        struct DimensionInfo dm;
        struct MonitorInfo mi;
        struct NameInfo ni;
        DisplayInfoHandle h = FindDisplayInfo(id);
        if (!h) continue;
        Printf("MODE %08lx na %lx", id, ModeNotAvailable(id));
        if (GetDisplayInfoData(h, (UBYTE *)&di, sizeof di, DTAG_DISP, 0))
            Printf(" props %08lx res %ld,%ld speed %ld sprres %ld,%ld", di.PropertyFlags, (LONG)di.Resolution.x,
                   (LONG)di.Resolution.y, (LONG)di.PixelSpeed, (LONG)di.SpriteResolution.x, (LONG)di.SpriteResolution.y);
        if (GetDisplayInfoData(h, (UBYTE *)&dm, sizeof dm, DTAG_DIMS, 0)) {
            Printf(" depth %ld", (LONG)dm.MaxDepth);
            rect("nom", &dm.Nominal);
            rect("max", &dm.MaxOScan);
            rect("video", &dm.VideoOScan);
            rect("txt", &dm.TxtOScan);
            rect("std", &dm.StdOScan);
        }
        if (GetDisplayInfoData(h, (UBYTE *)&mi, sizeof mi, DTAG_MNTR, 0)) {
            Printf(" rows %ld cclks %ld minrow %ld pos %ld,%ld", (LONG)mi.TotalRows, (LONG)mi.TotalColorClocks,
                   (LONG)mi.MinRow, (LONG)mi.ViewPosition.x, (LONG)mi.ViewPosition.y);
            rect("posrange", &mi.ViewPositionRange);
            Printf(" vres %ld,%ld ticks %ld,%ld defpos %ld,%ld pref %08lx compat %ld", (LONG)mi.ViewResolution.x,
                   (LONG)mi.ViewResolution.y, (LONG)mi.MouseTicks.x, (LONG)mi.MouseTicks.y,
                   (LONG)mi.DefaultViewPosition.x, (LONG)mi.DefaultViewPosition.y, mi.PreferredModeID,
                   (LONG)mi.Compatibility);
        }
        if (GetDisplayInfoData(h, (UBYTE *)&ni, sizeof ni, DTAG_NAME, 0)) Printf(" name \"%s\"", (LONG)ni.Name);
        Printf("\n");
    }
}

static void screen(ULONG id, LONG depth)
{
    ULONG err = 0;
    struct TagItem t[] = { { SA_DisplayID, id }, { SA_Depth, (ULONG)depth }, { SA_Behind, TRUE }, { SA_Quiet, TRUE },
                           { SA_ErrorCode, (ULONG)&err }, { TAG_DONE, 0 } };
    struct Screen *s = OpenScreenTagList(NULL, t);
    struct View *v;
    struct ViewPort *vp;
    UWORD *c;
    int i;
    if (!s) {
        Printf("SCREEN %08lx FAILED %ld\n", id, err);
        return;
    }
    MakeScreen(s);
    RethinkDisplay();
    Delay(10);
    v = ViewAddress();
    vp = &s->ViewPort;
    Printf("SCREEN %08lx view %ld,%ld modes %04lx vp %ld,%ld %ldx%ld modes %04lx vpmodeid %08lx\n", id,
           (LONG)v->DxOffset, (LONG)v->DyOffset, (LONG)v->Modes, (LONG)vp->DxOffset, (LONG)vp->DyOffset,
           (LONG)vp->DWidth, (LONG)vp->DHeight, (LONG)vp->Modes, GetVPModeID(vp));
    Printf("GFX system_bplcon0 %04lx current rows %ld cclks %ld monitor %s\n", (LONG)GfxBase->system_bplcon0,
           (LONG)GfxBase->current_tot_rows, (LONG)GfxBase->current_tot_cclks,
           (LONG)(GfxBase->current_monitor && GfxBase->current_monitor->ms_Node.xln_Name
                      ? GfxBase->current_monitor->ms_Node.xln_Name : "?"));
    /* the long frame list: MOVEs and WAITs, up to its end */
    c = v->LOFCprList ? v->LOFCprList->start : 0;
    Printf("COP");
    for (i = 0; c && i < 2000; i++, c += 2) {
        if (c[0] == 0xFFFF && c[1] == 0xFFFE) break;
        if (c[0] & 1) Printf(" W%04lx", (LONG)c[0]);
        else if ((c[0] < 0x180 || c[0] > 0x1BE) && (c[0] < 0x0E0 || c[0] > 0x0FE)) Printf(" %03lx=%04lx", (LONG)c[0], (LONG)c[1]);
    }
    Printf("\n");
    CloseScreen(s);
}

int main(void)
{
    LONG args[2] = { 0, 0 };
    struct RDArgs *rd = ReadArgs("ID/K,DEPTH/K/N", args, NULL);
    if (!rd) {
        PrintFault(IoErr(), "MonitorProbe");
        return 10;
    }
    Printf("graphics %ld.%ld\n", (LONG)GfxBase->LibNode.lib_Version, (LONG)GfxBase->LibNode.lib_Revision);
    if (args[0])
        screen(strtoul((char *)args[0], 0, 16), args[1] ? *(LONG *)args[1] : 2);
    else {
        monitors();
        modes();
    }
    FreeArgs(rd);
    return 0;
}
