/* GFOS face log: which tasks make the risky graphics.library and layers.library calls.
 * Workbench build wave 2, row 12 (amicradle-teams designs/WORKBENCH-REVIEW-SUMMARY-20261113.md, rule 5).
 *
 * The faces (library/graphics, library/layers) note six kinds of call, per task and per screen:
 *   direct-write   a screen's bitmap written by address, as something reports it (the faces don't see these;
 *                  whoever does calls facelog_note_n with FL_DIRECT_WRITE)
 *   ReadPixel      ReadPixel, ReadPixelLine8 and ReadPixelArray8 on a screen's RastPort
 *   LockBitMap     LockBitMapTags on a screen's bitmap
 *   OwnBlitter     OwnBlitter
 *   UCopList       a user copper list put on a ViewPort (UCopperListInit, or vp->UCopIns at MakeVPort)
 *   SuperBitMap    a super bitmap layer made (LAYERSUPER), SyncSBitMap, CopySBitMap
 *   LockLayerInfo  LockLayerInfo (the start of a walk of the Layer_Info list)
 * The screen is FL_SCREEN_WB, _PUBLIC or _CUSTOM where the call has one, else FL_SCREEN_NONE.
 *
 * Users: the screen-direct quirk rule (facelog_find: what a program did), and the direct-versus-composited
 * sweep harness (the LIST lines, one a task, call and screen with a count):
 *   face: task=<command name> call=<call> [screen=wb|public|custom] count=<n>
 *
 * Cheap when off: FACELOG_NOTE is a test of the log's pointer and one bit of its on mask, with no call. The
 * log is off until something turns it on (the FaceLog tool: FaceLog ON [calls], OFF, RESET, LIST [TO=file]).
 * When on, a note takes Forbid, finds the task's slot (the last one used first) and adds to a counter.
 *
 * One block for the whole machine, shared by both faces and the tool: a public SignalSemaphore named
 * FACELOG_SHARED_NAME, with the log after it. It is made by the first to attach and never freed (about 7 KB).
 * Up to FACELOG_TASKS tasks keep their own slot; the last slot, FACELOG_OTHERS, takes the rest. A slot is a
 * task (its address and a tag, so a new task at a dead one's address is a new slot) and its name, read once.
 *
 * This file is the whole module. Included plainly it is the portable core (no Amiga headers; the tests build
 * it for the host and for the 68040). With FACELOG_AMIGA it adds facelog_amiga_ops (the ops over exec, for a
 * face: no .data or .bss, SysBase from the caller). With FACELOG_TOOL it is the FaceLog command.
 * The faces own where they call it (Team 3, with the move of layers_comp.c and the ROM-core API).
 * Copyright (c) 2026 AmiCradle Team. MIT. */
#ifndef GFOS_FACELOG_C
#define GFOS_FACELOG_C

#include <stdint.h>
#include <string.h>

/* every call is static; a face that uses some of them gets no warning for the rest */
#ifdef __GNUC__
#define FACELOG_FN static __attribute__((unused))
#else
#define FACELOG_FN static
#endif

enum facelog_kind {
    FL_DIRECT_WRITE, FL_READPIXEL, FL_LOCKBITMAP, FL_OWNBLITTER, FL_UCOPLIST, FL_SUPERBITMAP, FL_LOCKLAYERINFO,
    FL_KINDS
};
enum facelog_screen { FL_SCREEN_NONE, FL_SCREEN_WB, FL_SCREEN_PUBLIC, FL_SCREEN_CUSTOM, FL_SCREENS };

#define FACELOG_SHARED_NAME "gfos.facelog"
#define FACELOG_MAGIC 0x46434C47UL          /* "FCLG" */
#define FACELOG_VERSION 1
#define FACELOG_TASKS 48
#define FACELOG_NAME 32
#define FACELOG_ALL ((1UL << FL_KINDS) - 1)
#define FACELOG_OTHERS "(other tasks)"

struct facelog_task {
    const void *task;
    uint32_t tag;
    uint32_t counts[FL_KINDS][FL_SCREENS];
    char name[FACELOG_NAME];
};

struct facelog {
    uint32_t magic, version, size;
    volatile uint32_t on;                   /* a bit for each enum facelog_kind */
    uint32_t ntasks;                        /* slots in use */
    uint32_t hint;                          /* the slot last used */
    uint32_t folded;                        /* notes counted in FACELOG_OTHERS */
    struct facelog_task t[FACELOG_TASKS];
};

/* What the log needs from exec. ctx is passed to each. */
struct facelog_ops {
    void *ctx;
    void *(*shared)(void *ctx, const char *name, uint32_t size);  /* the named block, made (cleared) if none */
    void (*forbid)(void *ctx);
    void (*permit)(void *ctx);
    const void *(*self)(void *ctx);                               /* FindTask(NULL) */
    uint32_t (*tag)(void *ctx, const void *task);                 /* cheap: tells a new task at an old address */
    void (*name)(void *ctx, const void *task, char *buf, int n);  /* the command name, else the task's name */
};

/* The faces' note: costs nothing more than this test while the log is off. */
#define FACELOG_ON(fl, kind) ((fl) && ((fl)->on & (1UL << (kind))))
#define FACELOG_NOTE(fl, ops, kind, screen) \
    do { if (FACELOG_ON(fl, kind)) facelog_note_n((fl), (ops), (kind), (screen), 1); } while (0)

FACELOG_FN const char *facelog_call_name(int kind)
{
    switch (kind) {     /* a switch, not a table of pointers: the faces have no .data */
    case FL_DIRECT_WRITE: return "direct-write";
    case FL_READPIXEL: return "ReadPixel";
    case FL_LOCKBITMAP: return "LockBitMap";
    case FL_OWNBLITTER: return "OwnBlitter";
    case FL_UCOPLIST: return "UCopList";
    case FL_SUPERBITMAP: return "SuperBitMap";
    case FL_LOCKLAYERINFO: return "LockLayerInfo";
    }
    return "?";
}

FACELOG_FN const char *facelog_screen_name(int screen)
{
    switch (screen) {
    case FL_SCREEN_WB: return "wb";
    case FL_SCREEN_PUBLIC: return "public";
    case FL_SCREEN_CUSTOM: return "custom";
    }
    return "";
}

/* The machine's log, made on first use; 0 when there is none to be had or it is another version's. */
FACELOG_FN struct facelog *facelog_attach(const struct facelog_ops *o)
{
    struct facelog *fl;
    o->forbid(o->ctx);
    fl = (struct facelog *)o->shared(o->ctx, FACELOG_SHARED_NAME, (uint32_t)sizeof(struct facelog));
    if (fl && !fl->magic) {
        fl->version = FACELOG_VERSION;
        fl->size = (uint32_t)sizeof(struct facelog);
        fl->magic = FACELOG_MAGIC;
    }
    o->permit(o->ctx);
    if (!fl || fl->magic != FACELOG_MAGIC || fl->version != FACELOG_VERSION || fl->size != sizeof(struct facelog))
        return 0;
    return fl;
}

FACELOG_FN void facelog_set(struct facelog *fl, uint32_t mask)
{
    if (fl) fl->on = mask & FACELOG_ALL;
}

FACELOG_FN void facelog_reset(struct facelog *fl, const struct facelog_ops *o)
{
    if (!fl) return;
    o->forbid(o->ctx);
    memset(fl->t, 0, sizeof fl->t);
    fl->ntasks = fl->hint = fl->folded = 0;
    o->permit(o->ctx);
}

/* n calls of kind on screen by the running task. Under Forbid, so two tasks can't take one slot. */
FACELOG_FN void facelog_note_n(struct facelog *fl, const struct facelog_ops *o, int kind, int screen, uint32_t n)
{
    const void *task;
    uint32_t tag, i;
    struct facelog_task *s = 0;
    if (!fl || kind < 0 || kind >= FL_KINDS || screen < 0 || screen >= FL_SCREENS || !(fl->on & (1UL << kind))) return;
    task = o->self(o->ctx);
    tag = o->tag ? o->tag(o->ctx, task) : 0;
    o->forbid(o->ctx);
    if (fl->hint < fl->ntasks && fl->t[fl->hint].task == task && fl->t[fl->hint].tag == tag
        && !(fl->hint == FACELOG_TASKS - 1 && fl->ntasks == FACELOG_TASKS))
        s = &fl->t[fl->hint];
    else {
        uint32_t own = fl->ntasks == FACELOG_TASKS ? FACELOG_TASKS - 1 : fl->ntasks;   /* slots of one task */
        for (i = 0; i < own; i++)
            if (fl->t[i].task == task && fl->t[i].tag == tag) { s = &fl->t[i]; fl->hint = i; break; }
        if (!s) {
            if (fl->ntasks < FACELOG_TASKS - 1) {
                s = &fl->t[fl->ntasks];
                s->task = task;
                s->tag = tag;
                if (o->name) o->name(o->ctx, task, s->name, FACELOG_NAME);
                s->name[FACELOG_NAME - 1] = 0;
                if (!s->name[0]) s->name[0] = '?';
                fl->hint = fl->ntasks++;
            } else {
                s = &fl->t[FACELOG_TASKS - 1];
                if (fl->ntasks < FACELOG_TASKS) {
                    memcpy(s->name, FACELOG_OTHERS, sizeof FACELOG_OTHERS);
                    fl->ntasks = FACELOG_TASKS;
                }
                fl->folded += n;
            }
        }
    }
    s->counts[kind][screen] += n;
    o->permit(o->ctx);
}

/* A copy of the slots in use (up to max) taken under Forbid; answers how many. */
FACELOG_FN int facelog_snapshot(struct facelog *fl, const struct facelog_ops *o, struct facelog_task *out, int max)
{
    int n;
    if (!fl || max <= 0) return 0;
    o->forbid(o->ctx);
    n = (int)fl->ntasks < max ? (int)fl->ntasks : max;
    memcpy(out, fl->t, (size_t)n * sizeof *out);
    o->permit(o->ctx);
    return n;
}

FACELOG_FN int facelog_same_name(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y) return 0;
    }
    return *a == *b;
}

/* Everything the tasks named name did (any case; every run of it), over all screens, into counts.
 * Answers how many slots matched. For the screen-direct rule. */
FACELOG_FN int facelog_find(struct facelog *fl, const struct facelog_ops *o, const char *name, uint32_t counts[FL_KINDS])
{
    int i, k, sc, hits = 0;
    for (k = 0; k < FL_KINDS; k++) counts[k] = 0;
    if (!fl || !name) return 0;
    o->forbid(o->ctx);
    for (i = 0; i < (int)fl->ntasks; i++)
        if (facelog_same_name(fl->t[i].name, name)) {
            hits++;
            for (k = 0; k < FL_KINDS; k++)
                for (sc = 0; sc < FL_SCREENS; sc++) counts[k] += fl->t[i].counts[k][sc];
        }
    o->permit(o->ctx);
    return hits;
}

/* One value of a line: no spaces or = in it, so a name like "Shell Process" stays one value. */
FACELOG_FN int facelog_put(char *buf, int at, int size, const char *s, int value)
{
    for (; *s; s++) {
        char c = *s;
        if (at >= size - 1) return -1;
        if (value && (c == ' ' || c == '=' || c == '\t' || c == '\n' || c == '\r')) c = '_';
        buf[at++] = c;
    }
    buf[at] = 0;
    return at;
}

FACELOG_FN int facelog_put_u(char *buf, int at, int size, uint32_t v)
{
    char d[12];
    int i = 0;
    do { d[i++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (i--) {
        if (at >= size - 1) return -1;
        buf[at++] = d[i];
    }
    buf[at] = 0;
    return at;
}

/* The LIST lines into buf (NUL ended): one a task, call and screen that has a count, in slot order. Answers the
 * length, or -1 when buf is too small (buf then holds what fitted, ending on a whole line). */
FACELOG_FN int facelog_list(struct facelog *fl, const struct facelog_ops *o, char *buf, int size)
{
    struct facelog_task t;
    int i, k, sc, at = 0, line;
    if (size <= 0) return -1;
    buf[0] = 0;
    if (!fl) return 0;
    for (i = 0; i < FACELOG_TASKS; i++) {
        o->forbid(o->ctx);
        if (i >= (int)fl->ntasks) { o->permit(o->ctx); break; }
        t = fl->t[i];
        o->permit(o->ctx);
        for (k = 0; k < FL_KINDS; k++)
            for (sc = 0; sc < FL_SCREENS; sc++) {
                if (!t.counts[k][sc]) continue;
                line = at;
                if ((at = facelog_put(buf, at, size, "face: task=", 0)) < 0
                    || (at = facelog_put(buf, at, size, t.name, 1)) < 0
                    || (at = facelog_put(buf, at, size, " call=", 0)) < 0
                    || (at = facelog_put(buf, at, size, facelog_call_name(k), 0)) < 0
                    || (sc != FL_SCREEN_NONE && ((at = facelog_put(buf, at, size, " screen=", 0)) < 0
                                                 || (at = facelog_put(buf, at, size, facelog_screen_name(sc), 0)) < 0))
                    || (at = facelog_put(buf, at, size, " count=", 0)) < 0
                    || (at = facelog_put_u(buf, at, size, t.counts[k][sc])) < 0
                    || (at = facelog_put(buf, at, size, "\n", 0)) < 0) {
                    buf[line] = 0;
                    return -1;
                }
            }
    }
    return at;
}

/* "ReadPixel,OwnBlitter" (any case, the LIST names) to an on mask; none or ALL is every call; 0 for a name
 * it doesn't know. */
FACELOG_FN uint32_t facelog_parse(const char *s)
{
    uint32_t mask = 0;
    char word[24];
    int n, k;
    if (!s || !*s || facelog_same_name(s, "ALL")) return FACELOG_ALL;
    while (*s) {
        for (n = 0; *s && *s != ',' && *s != ' '; s++)
            if (n < (int)sizeof word - 1) word[n++] = *s;
        word[n] = 0;
        while (*s == ',' || *s == ' ') s++;
        if (!n) continue;
        for (k = 0; k < FL_KINDS; k++)
            if (facelog_same_name(word, facelog_call_name(k))) break;
        if (k == FL_KINDS) return 0;
        mask |= 1UL << k;
    }
    return mask;
}

/* ---- on the Amiga: the ops over exec ---- */
#if defined(FACELOG_AMIGA) || defined(FACELOG_TOOL)
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/semaphores.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/exec.h>

/* ctx is SysBase: no global, as a face has no .data or .bss */
FACELOG_FN void *facelog_a_shared(void *ctx, const char *name, uint32_t size)
{
    struct ExecBase *SysBase = (struct ExecBase *)ctx;
    struct SignalSemaphore *s;
    uint32_t len = (uint32_t)strlen(name) + 1;
    Forbid();
    s = FindSemaphore((STRPTR)name);
    if (!s && (s = (struct SignalSemaphore *)AllocMem(sizeof *s + size + len, MEMF_PUBLIC | MEMF_CLEAR))) {
        char *n = (char *)(s + 1) + size;
        memcpy(n, name, len);
        s->ss_Link.ln_Name = n;
        s->ss_Link.ln_Pri = 0;
        AddSemaphore(s);                     /* inits it and puts it on the public list */
    }
    Permit();
    return s ? (void *)(s + 1) : 0;
}

FACELOG_FN void facelog_a_forbid(void *ctx) { struct ExecBase *SysBase = (struct ExecBase *)ctx; Forbid(); }
FACELOG_FN void facelog_a_permit(void *ctx) { struct ExecBase *SysBase = (struct ExecBase *)ctx; Permit(); }
FACELOG_FN const void *facelog_a_self(void *ctx) { struct ExecBase *SysBase = (struct ExecBase *)ctx; return FindTask(NULL); }

FACELOG_FN const UBYTE *facelog_a_cmd(const struct Task *t)
{
    const struct Process *p = (const struct Process *)t;
    const struct CommandLineInterface *cli;
    if (t->tc_Node.ln_Type != NT_PROCESS || !p->pr_CLI) return 0;
    cli = (const struct CommandLineInterface *)BADDR(p->pr_CLI);
    return cli->cli_CommandName ? (const UBYTE *)BADDR(cli->cli_CommandName) : 0;
}

FACELOG_FN uint32_t facelog_a_tag(void *ctx, const void *task)
{
    const struct Task *t = (const struct Task *)task;
    const UBYTE *cmd = facelog_a_cmd(t);
    (void)ctx;
    return (uint32_t)(uintptr_t)t->tc_Node.ln_Name ^ (cmd ? (uint32_t)(uintptr_t)cmd + cmd[0] : 0);
}

/* A Shell's program: its command name without the path; anything else: the task's name. */
FACELOG_FN void facelog_a_name(void *ctx, const void *task, char *buf, int n)
{
    const struct Task *t = (const struct Task *)task;
    const UBYTE *cmd = facelog_a_cmd(t);
    int i, from = 1, len;
    (void)ctx;
    buf[0] = 0;
    if (cmd && cmd[0]) {
        len = cmd[0];
        for (i = 1; i <= len; i++)
            if (cmd[i] == ':' || cmd[i] == '/') from = i + 1;
        for (i = 0; from <= len && i < n - 1; from++) buf[i++] = (char)cmd[from];
        buf[i] = 0;
    }
    if (!buf[0] && t->tc_Node.ln_Name) {
        for (i = 0; t->tc_Node.ln_Name[i] && i < n - 1; i++) buf[i] = t->tc_Node.ln_Name[i];
        buf[i] = 0;
    }
}

/* A face fills its own ops (kept in its library base) with this, then facelog_attach. */
FACELOG_FN void facelog_amiga_ops(struct facelog_ops *o, struct ExecBase *sysbase)
{
    o->ctx = sysbase;
    o->shared = facelog_a_shared;
    o->forbid = facelog_a_forbid;
    o->permit = facelog_a_permit;
    o->self = facelog_a_self;
    o->tag = facelog_a_tag;
    o->name = facelog_a_name;
}
#endif

/* ---- the FaceLog command ---- */
#ifdef FACELOG_TOOL
#include <proto/dos.h>

static const char facelog_version[] __attribute__((used)) = "$VER: FaceLog 1.0 (13.11.2026)";

int main(void)
{
    static const char tmpl[] = "CMD/A,CALLS,TO/K";
    LONG args[3] = { 0, 0, 0 };
    struct RDArgs *rd;
    struct facelog_ops o;
    struct facelog *fl;
    const char *cmd;
    int rc = RETURN_OK;
    (void)facelog_version;
    if (!(rd = ReadArgs((STRPTR)tmpl, args, NULL))) {
        PrintFault(IoErr(), (STRPTR)"FaceLog");
        PutStr((STRPTR)"FaceLog ON [ReadPixel,OwnBlitter,...|ALL] | OFF | RESET | LIST [TO=file]\n");
        return RETURN_ERROR;
    }
    facelog_amiga_ops(&o, SysBase);
    cmd = (const char *)args[0];
    if (!(fl = facelog_attach(&o))) {
        PutStr((STRPTR)"FaceLog: no log (out of memory, or another version's is in use)\n");
        rc = RETURN_FAIL;
    } else if (facelog_same_name(cmd, "ON")) {
        uint32_t m = facelog_parse((const char *)args[1]);
        if (!m) { PutStr((STRPTR)"FaceLog: calls are direct-write, ReadPixel, LockBitMap, OwnBlitter, UCopList, SuperBitMap, LockLayerInfo or ALL\n"); rc = RETURN_ERROR; }
        else facelog_set(fl, m);
    } else if (facelog_same_name(cmd, "OFF")) {
        facelog_set(fl, 0);
    } else if (facelog_same_name(cmd, "RESET")) {
        facelog_reset(fl, &o);
    } else if (facelog_same_name(cmd, "LIST")) {
        LONG size = 16384, len;
        char *buf = 0;
        for (;;) {
            if (!(buf = (char *)AllocVec((ULONG)size, MEMF_ANY))) { rc = RETURN_FAIL; break; }
            if ((len = facelog_list(fl, &o, buf, (int)size)) >= 0) break;
            FreeVec(buf);
            buf = 0;
            size *= 2;
        }
        if (buf) {
            BPTR fh = args[2] ? Open((STRPTR)args[2], MODE_NEWFILE) : Output();
            if (!fh) { PrintFault(IoErr(), (STRPTR)args[2]); rc = RETURN_ERROR; }
            else {
                if (len && Write(fh, buf, len) != len) rc = RETURN_ERROR;
                if (args[2]) Close(fh);
            }
            FreeVec(buf);
        }
    } else {
        PutStr((STRPTR)"FaceLog ON [calls] | OFF | RESET | LIST [TO=file]\n");
        rc = RETURN_ERROR;
    }
    FreeArgs(rd);
    return rc;
}
#endif

#endif
