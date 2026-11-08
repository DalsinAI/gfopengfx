/* SPDX-License-Identifier: MIT */
/* DriverCallLog: what a program asks of graphics.library's display calls.
 * Copyright (c) 2026 AmiCradle Team. MIT.
 *
 *   DriverCallLog [DUMP=addr,...] [LEN=n] [CALLS=first-last,...] [CMD=...]
 * patches OpenMonitor, CloseMonitor, FindDisplayInfo, NextDisplayInfo,
 * GetDisplayInfoData, GfxNew, GfxFree, GfxAssociate, GfxLookUp and the
 * private -738, -744 and -750 with SetFunction, runs CMD (a monitor driver,
 * say), takes the patches off and prints every call: its registers, the
 * memory a0, a1 and a2 point at before and a0's and a1's after, and the
 * result with the memory it points at (lvo 0 is a MonitorSpec's xln_Init). Then the display database's head and
 * each MonitorSpec's list, or with DUMP that memory instead, each with what
 * its longwords point at. It only observes. Run it on the ROM to be measured. */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <graphics/gfxbase.h>
#include <graphics/monitor.h>
#include <graphics/gfxnodes.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/rdargs.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/dos.h>
#include <stdlib.h>

extern struct GfxBase *GfxBase;

#define NCALLS 12
#define NSTUBS 13     /* the last is a MonitorSpec's xln_Init, as GfxNew left it */
#define NRECS 1200
#define NDUMP 192

struct rec {
    WORD lvo;
    APTR task;
    ULONG ret;
    ULONG r[15];
    ULONG stack[3];     /* what the caller pushed */
    ULONG res;
    UBYTE before[3][NDUMP];
    UBYTE after[2][NDUMP];
    UBYTE resmem[NDUMP];
    UBYTE valid;        /* bits 0-2 before, 3-4 after, 5 result */
};

static const WORD lvos[NCALLS] = { -660, -666, -672, -702, -714, -720, -726, -732, -738, -744, -750, -756 };
static ULONG orig[NSTUBS];
static struct rec *recs;
static ULONG nrecs;

/* each stub is 10 bytes: its index on the stack, then the common code. The
 * saved registers come at f[0..14]; log_entry's result takes the slot the
 * rts leaves through */
#define S(n) "	move.l #" #n ",-(%sp)\n	bra.w common\n"
__asm__("	.text\n	.even\n	.globl _stubs\n_stubs:\n"
        S(0) S(1) S(2) S(3) S(4) S(5) S(6) S(7) S(8) S(9) S(10) S(11) S(12)
        "common:\n"
        "	pea back(%pc)\n"
        "	subq.l #4,%sp\n"
        "	movem.l %d0-%d7/%a0-%a6,-(%sp)\n"
        "	move.l %sp,-(%sp)\n"
        "	jsr _log_entry\n"
        "	addq.l #4,%sp\n"
        "	move.l %d0,60(%sp)\n"
        "	movem.l (%sp)+,%d0-%d7/%a0-%a6\n"
        "	rts\n"
        "back:\n"
        "	movem.l %d0-%d7/%a0-%a6,-(%sp)\n"
        "	move.l %sp,-(%sp)\n"
        "	jsr _log_exit\n"
        "	addq.l #4,%sp\n"
        "	movem.l (%sp)+,%d0-%d7/%a0-%a6\n"
        "	addq.l #4,%sp\n"
        "	rts\n");
extern char stubs[];

static int readable(ULONG a)
{
    if (a >= 0xF80000 && a < 0x1000000) return 1;
    return a >= 0x400 && TypeOfMem((APTR)a) != 0;
}

static int snap(UBYTE *d, ULONG a)
{
    int i;
    if (!readable(a)) return 0;
    for (i = 0; i < NDUMP; i++) d[i] = ((UBYTE *)a)[i];
    return 1;
}

ULONG log_entry(ULONG *f)
{
    ULONG n = f[17], i;
    struct rec *r;
    Disable();
    i = nrecs < NRECS ? nrecs++ : NRECS;
    Enable();
    f[17] = i;
    if (i < NRECS) {
        int k;
        r = &recs[i];
        r->lvo = n < NCALLS ? lvos[n] : 0;
        r->task = FindTask(NULL);
        r->ret = f[18];
        for (k = 0; k < 15; k++) r->r[k] = f[k];
        for (k = 0; k < 3; k++) r->stack[k] = f[19 + k];
        for (k = 0; k < 3; k++)
            if (snap(r->before[k], f[8 + k])) r->valid |= 1 << k;
    }
    return orig[n];
}

void log_exit(ULONG *f)
{
    ULONG i = f[15];
    if (i < NRECS) {
        struct rec *r = &recs[i];
        int k;
        r->res = f[0];
        for (k = 0; k < 2; k++)
            if (snap(r->after[k], r->r[8 + k])) r->valid |= 8 << k;
        if (snap(r->resmem, f[0])) r->valid |= 32;
        /* GfxNew's MonitorSpec: through our stub when the driver calls its xln_Init */
        if (r->lvo == -660 && r->r[0] == 4 && f[0]) {
            struct ExtendedNode *x = (struct ExtendedNode *)f[0];
            orig[NCALLS] = (ULONG)x->xln_Init;
            *(APTR *)&x->xln_Init = (APTR)(stubs + 10 * NCALLS);
        }
    }
}

static void hex(const char *n, const UBYTE *d, int len)
{
    int i;
    Printf("  %s", (LONG)n);
    for (i = 0; i < len; i += 4)
        Printf(" %08lx", ((ULONG)d[i] << 24) | ((ULONG)d[i + 1] << 16) | ((ULONG)d[i + 2] << 8) | d[i + 3]);
    Printf("\n");
}

/* whether CALLS (none: every call) has call i */
static int wanted(const char *c, ULONG i)
{
    if (!c) return 1;
    while (*c) {
        char *e;
        ULONG a = strtoul(c, &e, 10), b = a;
        if (*e == '-') b = strtoul(e + 1, &e, 10);
        if (i >= a && i <= b) return 1;
        if (*e != ',') break;
        c = e + 1;
    }
    return 0;
}

static void report(const char *calls)
{
    ULONG i;
    for (i = 0; i < nrecs && i < NRECS; i++) {
        struct rec *r = &recs[i];
        if (!wanted(calls, i)) continue;
        Printf("CALL %ld lvo %ld task %08lx ret %08lx res %08lx in %08lx\n", i, (LONG)r->lvo, (LONG)r->task, r->ret, r->res, r->r[0]);
        Printf("  d0 %08lx d1 %08lx d2 %08lx d3 %08lx a0 %08lx a1 %08lx a2 %08lx a3 %08lx\n", r->r[0], r->r[1], r->r[2],
               r->r[3], r->r[8], r->r[9], r->r[10], r->r[11]);
        Printf("  stack %08lx %08lx %08lx\n", r->stack[0], r->stack[1], r->stack[2]);
        if (r->valid & 1) hex("a0", r->before[0], NDUMP);
        if (r->valid & 2) hex("a1", r->before[1], NDUMP);
        if (r->valid & 4) hex("a2", r->before[2], NDUMP);
        if (r->valid & 8) hex("a0'", r->after[0], NDUMP);
        if (r->valid & 16) hex("a1'", r->after[1], NDUMP);
        if (r->valid & 32) hex("res", r->resmem, NDUMP);
    }
    if (nrecs > NRECS) Printf("LOST %ld calls\n", nrecs - NRECS);
}

/* len bytes at a, and NDUMP bytes at each thing a longword there points at */
static void dump(const char *n, ULONG a, ULONG len)
{
    UBYTE d[NDUMP];
    ULONG o;
    Printf("DUMP %s %08lx\n", (LONG)n, a);
    if (!readable(a)) return;
    for (o = 0; o < len; o += NDUMP) {
        CopyMem((APTR)(a + o), d, NDUMP);
        Printf(" +%04lx", o);
        hex("", d, NDUMP);
    }
    for (o = 0; o + 4 <= len; o += 4) {
        ULONG p = *(ULONG *)(a + o);
        if (p >= 0x400 && readable(p) && snap(d, p)) {
            Printf(" +%04lx -> %08lx", o, p);
            hex("", d, NDUMP);
        }
    }
}

static void database(void)
{
    struct Node *n;
    dump("DisplayInfoDataBase", (ULONG)GfxBase->DisplayInfoDataBase, 256);
    for (n = GfxBase->MonitorList.lh_Head; n->ln_Succ; n = n->ln_Succ) {
        struct MonitorSpec *ms = (struct MonitorSpec *)n;
        Printf("MONITOR %s\n", (LONG)(n->ln_Name ? n->ln_Name : "?"));
        dump("MonitorSpec", (ULONG)ms, sizeof *ms);
        dump("its list head", (ULONG)ms->DisplayInfoDataBase.lh_Head, 128);
    }
}

int main(void)
{
    LONG args[4] = { 0, 0, 0, 0 };
    struct RDArgs *rd = ReadArgs("DUMP/K,LEN/K/N,CALLS/K,CMD/F", args, NULL);
    int i;
    if (!rd) {
        PrintFault(IoErr(), "DriverCallLog");
        return 10;
    }
    Printf("graphics %ld.%ld\n", (LONG)GfxBase->LibNode.lib_Version, (LONG)GfxBase->LibNode.lib_Revision);
    if (args[3]) {
        struct TagItem t[] = { { SYS_Input, 0 }, { SYS_Output, 0 }, { TAG_DONE, 0 } };
        LONG rc;
        recs = AllocMem(NRECS * sizeof(struct rec), MEMF_ANY | MEMF_CLEAR);
        if (!recs) {
            FreeArgs(rd);
            return 20;
        }
        t[0].ti_Data = (ULONG)Open("NIL:", MODE_OLDFILE);
        t[1].ti_Data = (ULONG)Open("NIL:", MODE_OLDFILE);
        for (i = 0; i < NCALLS; i++)
            orig[i] = (ULONG)SetFunction((struct Library *)GfxBase, lvos[i], (ULONG (*)())(stubs + 10 * i));
        rc = SystemTagList((STRPTR)args[3], t);
        for (i = 0; i < NCALLS; i++) SetFunction((struct Library *)GfxBase, lvos[i], (ULONG (*)())orig[i]);
        Printf("rc %ld\n", rc);
        report((const char *)args[2]);
        FreeMem(recs, NRECS * sizeof(struct rec));
    }
    if (args[0]) {
        const char *c = (const char *)args[0];
        while (*c) {
            char *e;
            dump("asked", strtoul(c, &e, 16), args[1] ? *(LONG *)args[1] : 256);
            if (*e != ',') break;
            c = e + 1;
        }
    } else
        database();
    FreeArgs(rd);
    return 0;
}
