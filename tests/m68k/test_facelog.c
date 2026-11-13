/* GFOS face log (gfosmodules/gfos_facelog.c): unit tests. A fake exec gives tasks, Forbid and the shared
 * block; the log's counts, names, kinds, screens, overflow, reset, the LIST lines and the cost when off are
 * checked. Built for the host and, by test_facelog.sh, for the 68040 under qemu-m68k.
 * Copyright (c) 2026 AmiCradle Team. MIT. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gfos_facelog.c"

static unsigned long checks, fails;
#define CHECK(c, what) do { checks++; if (!(c)) { fails++; printf("FAIL %s (line %d)\n", what, __LINE__); } } while (0)

/* ---- the fake exec ---- */
struct ftask { char name[40]; uint32_t tag; };
static struct ftask tasks[200];
static struct ftask *cur;
static int forbids, nest, self_calls, name_calls, shared_calls;
static unsigned char *block;
static uint32_t block_size;

static void *f_shared(void *ctx, const char *name, uint32_t size)
{
    (void)ctx;
    shared_calls++;
    if (strcmp(name, FACELOG_SHARED_NAME)) return 0;
    if (!block) { block = calloc(1, size); block_size = size; }
    return block_size >= size ? block : 0;
}
static void f_forbid(void *ctx) { (void)ctx; forbids++; nest++; }
static void f_permit(void *ctx) { (void)ctx; nest--; }
static const void *f_self(void *ctx) { (void)ctx; self_calls++; return cur; }
static uint32_t f_tag(void *ctx, const void *task) { (void)ctx; return ((const struct ftask *)task)->tag; }
static void f_name(void *ctx, const void *task, char *buf, int n)
{
    (void)ctx;
    const char *s = ((const struct ftask *)task)->name;
    int i;
    name_calls++;
    for (i = 0; s[i] && i < n - 1; i++) buf[i] = s[i];
    buf[i] = 0;
}
static const struct facelog_ops ops = { 0, f_shared, f_forbid, f_permit, f_self, f_tag, f_name };

static uint32_t count_of(struct facelog *fl, const char *name, int kind, int screen)
{
    struct facelog_task t[FACELOG_TASKS];
    int i, n = facelog_snapshot(fl, &ops, t, FACELOG_TASKS);
    uint32_t c = 0;
    for (i = 0; i < n; i++)
        if (!strcmp(t[i].name, name)) c += t[i].counts[kind][screen];
    return c;
}

int main(void)
{
    struct facelog *fl, *fl2;
    char line[160], all[4096];
    int i, n;

    /* attach: one block, shared by both faces; a block of another version is left alone */
    fl = facelog_attach(&ops);
    CHECK(fl != 0, "attach makes the shared block");
    fl2 = facelog_attach(&ops);
    CHECK(fl2 == fl, "a second attach (the other face) finds the same block");
    CHECK(fl->on == 0, "the log starts off");
    CHECK(nest == 0, "Forbid and Permit balance in attach");
    fl->version = FACELOG_VERSION + 1;
    CHECK(facelog_attach(&ops) == 0, "a block of another version is not used");
    fl->version = FACELOG_VERSION;

    /* off: the faces' check costs no call */
    snprintf(tasks[0].name, sizeof tasks[0].name, "Shapes");
    cur = &tasks[0];
    self_calls = forbids = 0;
    for (i = 0; i < 1000; i++) FACELOG_NOTE(fl, &ops, FL_READPIXEL, FL_SCREEN_WB);
    FACELOG_NOTE((struct facelog *)0, &ops, FL_OWNBLITTER, FL_SCREEN_NONE);
    CHECK(self_calls == 0 && forbids == 0, "off: no task lookup and no Forbid");
    CHECK(count_of(fl, "Shapes", FL_READPIXEL, FL_SCREEN_WB) == 0, "off: nothing counted");

    /* on for some calls only */
    facelog_set(fl, (1UL << FL_READPIXEL) | (1UL << FL_OWNBLITTER));
    for (i = 0; i < 5; i++) FACELOG_NOTE(fl, &ops, FL_READPIXEL, FL_SCREEN_WB);
    FACELOG_NOTE(fl, &ops, FL_OWNBLITTER, FL_SCREEN_NONE);
    FACELOG_NOTE(fl, &ops, FL_UCOPLIST, FL_SCREEN_CUSTOM);
    CHECK(count_of(fl, "Shapes", FL_READPIXEL, FL_SCREEN_WB) == 5, "ReadPixel counted per task and screen");
    CHECK(count_of(fl, "Shapes", FL_OWNBLITTER, FL_SCREEN_NONE) == 1, "OwnBlitter counted");
    CHECK(count_of(fl, "Shapes", FL_UCOPLIST, FL_SCREEN_CUSTOM) == 0, "a call kind left off is not counted");
    CHECK(nest == 0, "Forbid and Permit balance in a note");
    CHECK(name_calls == 1, "a task's name is read once, when it is first seen");

    /* every kind, two tasks, all screens */
    facelog_set(fl, FACELOG_ALL);
    snprintf(tasks[1].name, sizeof tasks[1].name, "Lemmings");
    cur = &tasks[1];
    for (i = 0; i < FL_KINDS; i++) FACELOG_NOTE(fl, &ops, i, FL_SCREEN_CUSTOM);
    facelog_note_n(fl, &ops, FL_DIRECT_WRITE, FL_SCREEN_PUBLIC, 40);
    cur = &tasks[0];
    FACELOG_NOTE(fl, &ops, FL_SUPERBITMAP, FL_SCREEN_PUBLIC);
    CHECK(count_of(fl, "Lemmings", FL_LOCKLAYERINFO, FL_SCREEN_CUSTOM) == 1, "LockLayerInfo counted");
    CHECK(count_of(fl, "Lemmings", FL_DIRECT_WRITE, FL_SCREEN_PUBLIC) == 40, "a count of n added at once");
    CHECK(count_of(fl, "Lemmings", FL_READPIXEL, FL_SCREEN_WB) == 0, "tasks keep their own counts");
    CHECK(count_of(fl, "Shapes", FL_SUPERBITMAP, FL_SCREEN_PUBLIC) == 1, "back to the first task's slot");
    CHECK(fl->ntasks == 2, "two tasks, two slots");

    /* a new task at a dead one's address (another tag) gets its own slot */
    snprintf(tasks[0].name, sizeof tasks[0].name, "Mandel");
    tasks[0].tag = 7;
    FACELOG_NOTE(fl, &ops, FL_READPIXEL, FL_SCREEN_WB);
    CHECK(count_of(fl, "Mandel", FL_READPIXEL, FL_SCREEN_WB) == 1, "a reused task address is a new task");
    CHECK(count_of(fl, "Shapes", FL_READPIXEL, FL_SCREEN_WB) == 5, "the old task's counts stay");

    /* the same program twice (two runs): find adds them up */
    snprintf(tasks[2].name, sizeof tasks[2].name, "Lemmings");
    cur = &tasks[2];
    FACELOG_NOTE(fl, &ops, FL_OWNBLITTER, FL_SCREEN_NONE);
    {
        uint32_t c[FL_KINDS];
        CHECK(facelog_find(fl, &ops, "lemmings", c) == 2, "find: both runs, any case");
        CHECK(c[FL_OWNBLITTER] == 2 && c[FL_DIRECT_WRITE] == 41, "find: counts added over runs and screens (1 custom + 40 public)");
        CHECK(facelog_find(fl, &ops, "NoSuch", c) == 0 && c[FL_OWNBLITTER] == 0, "find: no task, all zero");
    }

    /* the lines the sweep harness reads */
    n = facelog_list(fl, &ops, all, sizeof all);
    CHECK(n > 0, "LIST makes lines");
    CHECK(strstr(all, "face: task=Shapes call=ReadPixel screen=wb count=5\n") != 0, "LIST: ReadPixel on wb");
    CHECK(strstr(all, "face: task=Shapes call=OwnBlitter count=1\n") != 0, "LIST: no screen= for OwnBlitter");
    CHECK(strstr(all, "face: task=Lemmings call=direct-write screen=public count=40\n") != 0, "LIST: direct-write");
    CHECK(strstr(all, "face: task=Lemmings call=UCopList screen=custom count=1\n") != 0, "LIST: UCopList");
    CHECK(strstr(all, "face: task=Lemmings call=SuperBitMap screen=custom count=1\n") != 0, "LIST: SuperBitMap");
    CHECK(strstr(all, "face: task=Lemmings call=LockLayerInfo screen=custom count=1\n") != 0, "LIST: LockLayerInfo");
    CHECK(strstr(all, "face: task=Lemmings call=LockBitMap screen=custom count=1\n") != 0, "LIST: LockBitMap");
    CHECK(strstr(all, "count=0") == 0, "LIST: no zero lines");
    CHECK(facelog_list(fl, &ops, line, 20) < 0, "LIST: a buffer too small says so");

    /* a name with a space or = is kept readable as one value */
    snprintf(tasks[3].name, sizeof tasks[3].name, "Shell Process=1");
    cur = &tasks[3];
    FACELOG_NOTE(fl, &ops, FL_OWNBLITTER, FL_SCREEN_NONE);
    facelog_list(fl, &ops, all, sizeof all);
    CHECK(strstr(all, "task=Shell_Process_1 call=OwnBlitter") != 0, "LIST: spaces and = in a name become _");

    /* more tasks than slots: the last slot holds the rest, and says how many were folded in */
    for (i = 10; i < 10 + FACELOG_TASKS + 20; i++) {
        snprintf(tasks[i].name, sizeof tasks[i].name, "T%d", i);
        cur = &tasks[i];
        FACELOG_NOTE(fl, &ops, FL_OWNBLITTER, FL_SCREEN_NONE);
    }
    CHECK(fl->ntasks == FACELOG_TASKS, "the table fills and stops");
    CHECK(fl->folded > 0, "tasks past the table are folded");
    CHECK(count_of(fl, FACELOG_OTHERS, FL_OWNBLITTER, FL_SCREEN_NONE) == fl->folded, "the rest counted in (other tasks)");
    CHECK(nest == 0, "Forbid and Permit balance");

    /* parse the call names (FaceLog ON ReadPixel,OwnBlitter) */
    CHECK(facelog_parse(0) == FACELOG_ALL && facelog_parse("") == FACELOG_ALL && facelog_parse("ALL") == FACELOG_ALL,
          "parse: none or ALL is every call");
    CHECK(facelog_parse("readpixel,OwnBlitter") == ((1UL << FL_READPIXEL) | (1UL << FL_OWNBLITTER)), "parse: a list, any case");
    CHECK(facelog_parse("direct-write") == (1UL << FL_DIRECT_WRITE), "parse: direct-write");
    CHECK(facelog_parse("ReadPixel,Nonsense") == 0, "parse: an unknown name is an error (0)");
    for (i = 0; i < FL_KINDS; i++) CHECK(facelog_parse(facelog_call_name(i)) == (1UL << i), "parse: each call's own name");

    /* reset: counts and tasks gone, on stays as it was */
    facelog_reset(fl, &ops);
    CHECK(fl->ntasks == 0 && fl->folded == 0, "reset empties the table");
    CHECK(fl->on == FACELOG_ALL, "reset keeps on");
    CHECK(facelog_list(fl, &ops, all, sizeof all) == 0 && all[0] == 0, "reset: LIST is empty");
    facelog_set(fl, 0);
    CHECK(fl->on == 0, "off again");
    CHECK(nest == 0, "Forbid and Permit balance at the end");

    /* bad arguments from a face never write outside the counts */
    facelog_set(fl, FACELOG_ALL);
    cur = &tasks[0];
    facelog_note_n(fl, &ops, FL_KINDS, FL_SCREEN_WB, 1);
    facelog_note_n(fl, &ops, -1, FL_SCREEN_WB, 1);
    facelog_note_n(fl, &ops, FL_READPIXEL, 9, 1);
    CHECK(fl->ntasks == 0, "a bad call kind or screen is ignored");

    printf("%lu checks, %lu failed\n", checks, fails);
    return fails != 0;
}
