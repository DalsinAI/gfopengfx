/* SPDX-License-Identifier: MIT */
/* OverlayTest: gfopenoverlay.library on a real screen. Opens a window on the front screen, an
 * overlay in it, and prints the place record after each step: opened; window moved; window sized;
 * a second window over half of it; that window gone; the box set to a part; closed. Each step
 * checks what the record must say; the last line is "N/M overlay steps pass".
 *   OverlayTest            (the Workbench screen)
 * Copyright (c) 2026 AmiCradle Team. MIT. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/dos.h>
#include <proto/gfopenoverlay.h>
#include <stdio.h>

struct Library *GFOpenOverlayBase;
static int steps, fails;

static void rec(const char *what, struct LayerOverlayPlace *p, int ok)
{
    ULONG s1, s2;
    struct LayerOverlayPlace c;
    int i, tries = 0;
    do {
        s1 = p->seq;
        c = *p;
        s2 = p->seq;
    } while (((s1 & 1) || s1 != s2) && ++tries < 1000);
    steps++;
    if (!ok) fails++;
    printf("%s %s: v%u flags %lx%s%s%s%s fmt %lu box %d,%d-%d,%d aspect %u rects %u",
           ok ? "ok  " : "FAIL", what, c.version, (unsigned long)c.flags,
           c.flags & OVPF_SHOWN ? " SHOWN" : "", c.flags & OVPF_WHOLE ? " WHOLE" : "",
           c.flags & OVPF_COMPOSITED ? " COMPOSITED" : "", c.flags & OVPF_TOOMANY ? " TOOMANY" : "",
           (unsigned long)c.format, c.box.MinX, c.box.MinY, c.box.MaxX, c.box.MaxY, c.aspect, c.nrects);
    for (i = 0; i < c.nrects && i < 4; i++)
        printf(" [%d,%d-%d,%d]", c.rects[i].MinX, c.rects[i].MinY, c.rects[i].MaxX, c.rects[i].MaxY);
    printf("\n");
}

static int whole(struct LayerOverlayPlace *p)
{
    return (p->flags & OVPF_WHOLE) && p->nrects == 1 && p->rects[0].MinX == p->box.MinX && p->rects[0].MaxY == p->box.MaxY;
}

int main(void)
{
    struct Window *w, *f;
    APTR ov;
    struct LayerOverlayPlace *p;
    struct TagItem none[] = { { TAG_DONE, 0 } };
    struct TagItem part[] = { { OVA_Left, 10 }, { OVA_Top, 10 }, { OVA_Width, 100 }, { OVA_Height, 80 }, { TAG_DONE, 0 } };
    struct TagItem fill[] = { { OVA_Aspect, OVASPECT_FILL }, { TAG_DONE, 0 } };
    struct TagItem keep[] = { { OVA_PicWidth, 640 }, { OVA_PicHeight, 480 }, { TAG_DONE, 0 } };
    LONG bw, bh;

    if (!(GFOpenOverlayBase = OpenLibrary(GFOPENOVERLAY_NAME, GFOPENOVERLAY_VERSION))) {
        printf("no gfopenoverlay.library 1 (it needs layers.library 47.111 or later)\n");
        return 20;
    }
    w = OpenWindowTags(NULL, WA_Left, 40, WA_Top, 40, WA_InnerWidth, 320, WA_InnerHeight, 256,
                       WA_Title, (ULONG)"OverlayTest", WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_SizeGadget, TRUE,
                       WA_MinWidth, 50, WA_MinHeight, 40, WA_MaxWidth, ~0, WA_MaxHeight, ~0, TAG_DONE);
    if (!w) { printf("no window\n"); CloseLibrary(GFOpenOverlayBase); return 20; }
    if (!(ov = OpenOverlay(w, keep))) { printf("FAIL OpenOverlay\n"); CloseWindow(w); CloseLibrary(GFOpenOverlayBase); return 20; }
    p = OverlayPlace(ov);
    bw = w->Width - w->BorderLeft - w->BorderRight;
    bh = w->Height - w->BorderTop - w->BorderBottom;
    rec("opened: the inner area, whole", p, p && p->magic == OVP_MAGIC && p->version >= 2 && whole(p)
        && p->box.MinX == w->LeftEdge + w->BorderLeft && p->box.MaxX - p->box.MinX + 1 == bw && p->box.MaxY - p->box.MinY + 1 == bh);
    MoveWindow(w, 30, 20);
    Delay(10);
    rec("moved by 30,20: the box with it", p, whole(p) && p->box.MinX == w->LeftEdge + w->BorderLeft);
    SizeWindow(w, 100, 50);
    Delay(10);
    SetOverlayAttrs(ov, none);
    rec("sized by 100,50, then the inner area again", p, whole(p) && p->box.MaxX - p->box.MinX + 1 == bw + 100);
    f = OpenWindowTags(NULL, WA_Left, w->LeftEdge + w->Width / 2, WA_Top, w->TopEdge, WA_Width, w->Width, WA_Height, w->Height / 2,
                       WA_Title, (ULONG)"in front", TAG_DONE);
    Delay(10);
    rec("a window over part of it: shown, not whole", p, f && (p->flags & OVPF_SHOWN) && !(p->flags & OVPF_WHOLE));
    if (f) CloseWindow(f);
    Delay(10);
    rec("that window gone: whole again", p, whole(p));
    SetOverlayAttrs(ov, part);
    rec("the box set to 100x80 at 10,10 of the inner area", p, whole(p) && p->box.MaxX - p->box.MinX + 1 == 100
        && p->box.MinX == w->LeftEdge + w->BorderLeft + 10);
    SetOverlayAttrs(ov, fill);
    rec("aspect FILL, the box kept", p, p->aspect == OVASPECT_FILL && p->box.MaxX - p->box.MinX + 1 == 100);
    CloseOverlay(ov);
    CloseWindow(w);
    CloseLibrary(GFOpenOverlayBase);
    printf("%d/%d overlay steps pass\n", steps - fails, steps);
    return fails ? 10 : 0;
}
