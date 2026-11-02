/* Test-only: exec/libraries.h as far as include/graphics/gfxdispatch.h uses
 * it (struct Library, for GFX_DISPATCH_AVAILABLE), for
 * tests/m68k/test_gfx_dispatch.sh. Written from the published layout; no
 * NDK text. MIT, AmiCradle Team. */
#ifndef EXEC_LIBRARIES_H
#define EXEC_LIBRARIES_H
#include <exec/nodes.h>
struct Library {
    struct Node lib_Node;
    UBYTE lib_Flags, lib_pad;
    UWORD lib_NegSize, lib_PosSize, lib_Version, lib_Revision;
    APTR lib_IdString;
    ULONG lib_Sum;
    UWORD lib_OpenCnt;
};
#endif
