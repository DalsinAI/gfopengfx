# GFOpenGfx graphics.library: the display

This directory is the display area of GFOpenGfx's `graphics.library` (see `library/graphics/README.md` for the face and `OWNERS.md` for who owns what). It is clean-room work from the NDK 3.2 headers, the autodocs, the Hardware Reference Manual and OS 3.2's measured behaviour. It covers 43 LVOs:

- Views and ViewPorts: InitView, InitVPort, MakeVPort, MrgCop, LoadView, FreeVPortCopLists, FreeCopList, FreeCprList, ScrollVPort, ChangeVPBitMap, AllocDBufInfo, FreeDBufInfo and CalcIVG.
- User copper lists: UCopperListInit, CWait, CMove and CBump. CINIT, CWAIT, CMOVE and CEND are the NDK macros over these.
- The display database: FindDisplayInfo, NextDisplayInfo, GetDisplayInfoData, BestModeIDA, ModeNotAvailable, GetVPModeID and CoerceMode.
- GfxNew, GfxFree, GfxAssociate, GfxLookUp, VideoControl, OpenMonitor, CloseMonitor and SetChipRev.
- The beam: VBeamPos, WaitTOF and WaitBOVP.
- Sprites: GetSprite, FreeSprite, ChangeSprite, MoveSprite, AllocSpriteDataA, FreeSpriteData, GetExtSpriteA and ChangeExtSpriteA.

| File | What |
| --- | --- |
| `display_types.h` | The NDK layouts in stdint types (`gd_` prefix), with 68k static_asserts: View, ViewPort, CopList, CopIns, UCopList, cprlist, copinit, ViewExtra, ViewPortExtra, MonitorSpec, SpecialMonitor, DBufInfo, the DisplayInfo records, SimpleSprite and ExtSprite. It also has the register offsets and the tags. |
| `display.h` | The calls in plain C arguments (`gd_<Call>(env, ...)`), plus `gd_init`, `gd_exit`, `gd_vblank` and `gfx_hook_vp_colors`. |
| `display_int.h` | The core's private state. It is kept in `gfx_private.area[GFX_AREA_DISPLAY]`, because the library has no .bss. |
| `display_view.cpp` | Lean C++. It turns a ViewPort into copper (DspIns), merges the lists (MrgCop), and handles LoadView, ScrollVPort, user copper lists, double buffering and the vertical blank. |
| `display_hw.cpp` | Lean C++: the beam, WaitTOF, the chip revision and the sprites. |
| `display_db.cpp` | Lean C++: the display database, monitors, extended nodes, VideoControl and `gd_init`. |
| `display_lvo.c` | The 43 `gfx_lvo_` register entries, in graphics.lvo order with A6 last. `gfx_init_display` installs GfxBase->vbsrv, the vertical blank server, through the env. |
| `graphics.sources` | The files that `library/graphics/build-amiga.sh` builds into the library. |
| `build-amiga.sh` | Builds `DisplayABITest` with the os32-gcc16 stove. |
| `tests/m68k/test_gfx_display.sh` | The qemu test (below). |
| `tests/DisplayABITest.c` | One script run through OS 3.2's graphics.library and then through gfopengfx.library, compared line by line. `PROBE` prints OS 3.2's answers only. |
| `tests/DriverCallLog.c` | Patches the display calls with SetFunction, runs a program (a monitor driver) and prints every call: registers, the memory they point at, the result. `build-amiga.sh` builds it. |
| `tests/MonitorProbe.c` | On any ROM, after the monitor drivers have run: every MonitorSpec, every record of the database (MON= one monitor's, by ID), and one mode's screen with its copper list (ID=, FRONT). `build-amiga.sh` builds it. Its OS 3.2.3 output is in `tests/m68k/display_os323_*.txt`. |

## How a ViewPort becomes copper

MakeVPort writes the ViewPort's DspIns in three blocks:

1. **The gap.** IVG lines before the ViewPort's first line, it sets the window, the fetch, the modulos, BPLCON1–4 (with BPLCON0 having no planes yet), the bitplane pointers and the colours.
2. **The start.** A WAIT on the first line, then BPLCON0 with its planes.
3. **The end.** A WAIT on the line after the last, then BPLCON0 with no planes.

On AGA the colours are loaded bank by bank: BPLCON3 with the bank number, the high nibbles, then LOCT and the low nibbles. On AGA it loads at least 32 entries, or as many as the ViewPort can show: its depth, BPLAM's XOR and PF2's base. With FULLPALETTE it loads them all.

MrgCop gathers every visible ViewPort's DspIns, SprIns, ClrIns and UCopIns:

- DspIns WAITs are frame lines. The other lists' WAITs count from the ViewPort's top.
- It sorts them stably by beam position and adds the `0xFFDF` wait before line 256, and again before line 512 (a 31 kHz monitor's frame is longer than 512 lines).
- It writes one hardware list per field. A laced View gets a long-frame list and a short-frame list.
- Lists that are big enough are reused while the View is not on display. A View on display always gets new lists, since the copper may be running the old ones; they are switched to at the next vertical blank, and the old ones are freed after it.

ScrollVPort, ChangeVPBitMap and colour changes (`gfx_hook_vp_colors`) rewrite the gap block in place in each hardware list. They do not need a new MrgCop.

The copper starts at copinit, which is in Chip RAM: FMODE, a blank window, BPLCON2, the eight sprite pointers, then a jump to COP2LC, which holds the View's list for the coming field. LoadView sets COP1LC and COP2LC, and also BEAMCON0 on ECS and AGA; for a driver's monitor with a SpecialMonitor it calls the driver's `do_monitor` instead, once when the monitor changes, which programs the beam (see below). The vertical blank server picks a laced View's list from VPOSR's LOF bit.

The horizontal position follows the HRM:

- Lores data shows from 2·DDFSTRT+17, and hires and super hires from 2·DDFSTRT+9 (in lores pixels).
- AGA's wider fetch (FMODE bits 0-1) takes 2 or 4 words of each plane a cycle. AGA uses the widest fetch the ViewPort's bitmaps allow: planes and rows on 64 bits give 4× (FMODE 3), on 32 bits 2× (FMODE 1), else 1×. This leaves the most chip bus cycles to the CPU and the blitter. A ViewPort deeper than the 1× fetch shows (hires over 4 planes, super hires over 2) needs 2× (hires) or 4× (super hires), so every resolution shows 8 planes on AGA; with its bitmap off that width MakeVPort answers MVP_NO_DISPLAY. AllocBitMap's displayable bitmaps are on 64 bits on AGA.
- The fetch cycle is 8 colour clocks, except lores at 2× (16) and 4× (32) and hires at 4× (16). DDFSTOP is the last cycle's start at any width (HRM; and OS 3.2.3's DblPAL Low Res, hires at 4×: 0x18-0x68 for 6 cycles).
- At 2× and 4× the data shows from 2·DDFSTRT+17, so a full screen of any resolution fetches from 0x38. As in OS 3.2.3's lists, DDFSTRT is then a whole number of cycles after 0x18, and the cycle that would start at the window's right edge is fetched too: a 640 pixel PAL High Res screen at 4× fetches 0x38-0xD8 (11 cycles of 64 pixels, BPL1MOD -8), as OS 3.2.3's. Each playfield's first fetch is on its width, and BPLCON1 (AGA's 8 bits, up to 64 lores pixels) carries the rest. A wider fetch that would start before 0x18 (the HRM's first DDFSTRT; a ViewPort at the left edge, scrolled) falls back to a narrower one.
- On ECS and AGA every ViewPort's first block sets DIWHIGH after DIWSTRT and DIWSTOP (whose writes turn it off), so the window's bits 8-10 are exact: OS 3.2.3's PAL Workbench writes 0x2C81, 0x2CC1, 0x2100, and so do we. OCS keeps the implied bits (V8 of the start 0, of the stop not V7, H8 of the stop 1).
- Every AGA ViewPort's first block sets FMODE: its fetch with copinit's sprite width. A sprite width change (ChangeExtSpriteA) rewrites the shown ViewPorts' blocks.
- Each playfield's scroll (BPLCON1) and the fetch start are worked out so the first word fetched is the one holding RxOffset at DIW start.
- DDFSTOP covers the window, in whole fetch units, and is clamped at 0xD8.
- OCS and ECS scroll in lores pixels, so a hires RxOffset is rounded down to an even number.

## 31 kHz monitors: Multiscan, DblNTSC and DblPAL

These are as OS 3.2.3's drivers make them, measured with `tests/MonitorProbe.c` on an OS 3.2.3 A1200 lab (3 November 2026). The measurements are kept in `tests/m68k/display_os323_modes.txt` (every record) and `display_os323_screens.txt` (the MonitorSpecs and the copper lists of six screens).

- **The database.** A driver adds its own MonitorSpec to GfxBase->MonitorList. While "multiscan.monitor", "DblNTSC.monitor" or "DblPAL.monitor" is there, its modes are in the database: Multiscan 24 (0x31000), DblNTSC 16 (0x91000), DblPAL 16 (0xA1000). Each has OS 3.2.3's DisplayInfo, DimensionInfo, MonitorInfo and NameInfo, made by rules from the measured records. MonitorInfo's rows, colour clocks and first row are the driver's MonitorSpec's. With no driver, the database is as before (90 modes).
- **The modes.** Every line is progressive. A ViewPort's LACE means "No Flicker", one row a line. Without LACE each row is shown on two lines: FMODE's BSCAN2, with BPL1MOD going back over the row and BPL2MOD on to the next. Sprites are scan doubled (SSCAN2). BPLCON3 has EXTBLKEN, and hires sprites unless the ColorMap says otherwise. DblNTSC's and DblPAL's bitplanes run a resolution faster than the mode says (Low Res is hires bitplanes at 70 ns); Multiscan's run at the resolution the mode says.
- **The beam.** HTOTAL, the syncs, the blanks and BEAMCON0 (0x1B88) are the driver's: LoadView calls its SpecialMonitor's `do_monitor` when the View's monitor changes. A built-in monitor still gets BEAMCON0 from its MonitorSpec.
- **Checked against OS 3.2.3.** For DblPAL High Res No Flicker and Low Res, Multiscan Productivity, DblNTSC High Res No Flicker, and PAL High Res laced and plain, MakeVPort sets OS 3.2.3's DIWSTRT, DIWSTOP, DIWHIGH, DDFSTRT, DDFSTOP, BPLCON1, both modulos, FMODE, BPLCON0 and BPLCON3.
- **Known differences** from those lists. On PAL, OS 3.2.3's BPLCON0 has no ECSENA, its BPLCON3 asks for lores sprites (0x0C40) where ours leaves ECS's default, and its BPLCON2 has KILLEHB at any depth (ours only at 6 planes, where it matters).

## Monitor drivers

OS 3.2.3's drivers in `SYS:Storage/Monitors` were measured with `tests/DriverCallLog.c` (SetFunction, 8 November 2026). DblNTSC, DblPAL, Multiscan, Euro36, Euro72, Super72, NTSC and A2024 all call these:

- **`GfxBase->DisplayInfoDataBase` is a tree.** A record (`gd_dbnode`, 56 bytes) has succ, pred, first child, parent, an ID, then TAG_MORE and its first data record. The root's children are monitors (ID: the monitor's ID >> 16), their children modes (ID: the display ID). A driver allocates its monitor's and modes' records and links them in itself, walking the tree to do so. Until 47.103 that field pointed at the array of IDs, so the walk ran through open bus for ever and the boot stopped. The built-in modes' records are there for the walk; they have no data, and the calls still work them out.
- **A data record** is a QueryHeader (StructID, DisplayID, TAG_SKIP, Length), then Length 8-byte units, then `{TAG_MORE, next}` or `{0, 0}`. -744 (`a0` handle or 0, `a1` buffer, `d0` size, `d1` tag, `d2` ID when the handle is 0) adds a copy and answers `d0`. -750 (the same registers) changes one and answers the bytes written; a MonitorInfo keeps its header and Mspc, so it answers 20 fewer.
- **A MonitorSpec's `xln_Init`.** GfxNew leaves a routine there and the driver calls it: `a0` the MonitorSpec, `d0` request flags (NTSC 1, PAL 2, SPECIAL 4, A2024 8), answering `d0`. It sets the flags and standard timing and, with SPECIAL, GfxNew's a SpecialMonitor. The driver overwrites most of the timing.
- **FindDisplayInfo, NextDisplayInfo, GetDisplayInfoData, ModeNotAvailable and OpenMonitor** take a driver's records first, so its own values replace the tables above for its modes. NextDisplayInfo goes by ID, as before, not in the tree's order.

## Edge cases

| Case | What we do | Source |
| --- | --- | --- |
| InitView | Every byte 0, then DxOffset 0x81 and DyOffset 0x2C (STANDARD_VIEW_X/Y) | ABI test (INITVIEW) |
| InitVPort | Every byte 0 | ABI test (INITVPORT) |
| MakeVPort with no RasInfo or bitmap, or too many planes for the chips (OCS and ECS: lores 6, hires 4, super hires 2; AGA: 8 in each, hires at 2× and super hires at 4×), or a bitmap off the width of the fetch its planes need | MVP_NO_DISPLAY | documented codes; the limits are the chips' |
| DimensionInfo's MaxDepth | AGA: 8 in every resolution, 6 for EHB. OCS and ECS: lores 5 (6 with EHB, HAM or dual playfield), hires 4, super hires 2 | the chips' fetch |
| MakeVPort below the frame | MVP_OFF_BOTTOM, with the old DspIns left alone | ABI test (VIEW offbottom) |
| VP_HIDE | MakeVPort answers MVP_OK and MrgCop leaves the ViewPort out | documented; ABI test |
| A ViewPort wholly clipped (by the monitor or a DisplayClip) | MVP_OK with no DspIns | ours |
| DyOffset in a laced ViewPort | In laced rows: halved and rounded down, so an odd DyOffset starts on the same frame line as DyOffset − 1. Even rows are in the long frame, odd rows in the short | ours; test |
| A View with no ViewPorts | MrgCop gives lists that only end the frame | ABI test (VIEW empty) |
| MrgCop out of memory | MCOP_NO_MEM, with the View's old lists kept | documented |
| Six planes without EXTRA_HALFBRITE, HAM or dual playfield | On AGA, BPLCON2's KILLEHB, so 64 colours; OCS and ECS have no such bit and show them half-bright | HRM (KILLEHB is AGA's); ABI test |
| ChangeVPBitMap's messages | dbi_SafeMessage and then dbi_DispMessage, both at the next vertical blank: from that field the copper shows the new bitmap and no longer reads the old one. (Until 1 November 2026 DispMessage came a field later, so double buffering the RKM's way, waiting for DispMessage before the next ChangeScreenBuffer, ran at half the field rate) | autodoc; AROS replies both at that blank; test |
| CoerceMode with AVOID_FLICKER | The non-interlaced modes are tried first, then the interlaced | autodoc (the flag); ABI test for the order |
| GfxFree of a node that is not one of GfxNew's (the type, subsystem or library not GfxNew's) | Left alone | ABI test |
| ScrollVPort on a ViewPort that is not shown | MakeVPort makes it again when it is merged | ABI test |
| MOVEs in a user copper list before its first WAIT | At the ViewPort's top line | ABI test |
| CBump past a CopList's end | Another CopList is chained on. With no memory, the last instruction is written over | documented / ours |
| UCopperListInit on a UCopList that already has a list | That list is reused when it is big enough and not chained; otherwise a new one | ABI test (UCOP init) |
| FreeVPortCopLists | Frees DspIns, SprIns, ClrIns and the UCopList, including the UCopList structure itself | documented; ABI test (UCOP freed) |
| SpritePriorities 0 | BPLCON2 0x24, as OS 3.2 does | ABI test |
| MoveSprite | x in the ViewPort's pixels and y in its lines; HSTART is the View's left edge plus x − 1 | ABI test (SPRITE moved) |
| MoveSprite on AGA in a hires or super hires ViewPort | HSTART in 35 ns pixels: SPRxCTL's SH1 (bit 4) and SH0 (bit 3) as well, so the sprite moves by the ViewPort's own pixels | AGA's SPRxCTL; test |
| Sprite width (SPRITEA_Width 32 or 64) | One width for all eight sprites, as the chips fetch them (FMODE's SPR32 and SPAGEM, GfxBase's SpriteWidth and SpriteFMode). GetExtSpriteA and ChangeExtSpriteA set it to the sprite's; SPRxCTL is the first word of the sprite's second fetch (word 2 or 4). On OCS and ECS a wider sprite is refused: GetExtSpriteA answers -1, ChangeExtSpriteA 0. (Until 3 November 2026 GetExtSpriteA left the width as it was, so a wide sprite showed at 16) | HRM (AGA's sprite fetch); test |
| GetSprite of a sprite that is taken, or -1 with none free | -1 | documented |
| AllocSpriteDataA defaults | num -1, height from the bitmap, word width 1, posctldata 8-aligned in Chip RAM | ABI test (SPRDATA) |
| GetExtSpriteA with old-format data and no OutputHeight | NULL: old data does not say how tall it is | ABI test |
| GfxNew | xln_Library is GfxBase; xln_Init is a MonitorSpec's routine (see "Monitor drivers"), 0 for the other types (OS 3.2 puts a routine there too); a MonitorSpec's DisplayInfoDataBase is NewList'd | ABI test (GFXNEW); DriverCallLog |
| VideoControl with an unknown tag | Answers 1 and goes on | ABI test (VC unknown) |
| NextDisplayInfo of an ID not in the database | The next ID above it | ABI test (NEXT unknown) |
| ModeNotAvailable of an unknown monitor | DI_AVAIL_NOMONITOR | ABI test (NOTAVAIL) |
| GetDisplayInfoData DTAG_NAME for the default monitor's modes | 0: only NTSC and PAL modes have names | ABI test (MODE) |
| GetDisplayInfoData DTAG_VEC, or an unknown ID | 0 | ABI test (VEC, UNKNOWN) |
| GetDisplayInfoData with a short buffer | Copies as many bytes as fit and answers that count | documented; ABI test (SHORT) |
| PaletteRange | 4096 on OCS and ECS; 0xFFFF on AGA | ABI test |
| MonitorInfo ViewResolution | 44, 44 | ABI test (MNTR) |
| BestModeIDA with no DIPFMustNotHave | The default (SPECIAL_FLAGS) minus whatever DIPFMustHave asks for | documented default; the subtraction is ABI test |
| BestModeIDA ties | Scored in order: fits the size, closest aspect, closest size, then the asked-for monitor | ours |
| GetVPModeID of a ViewPort with no ColorMap or VPModeID | Its Modes on the default monitor | ABI test (VPMODEID old) |
| CloseMonitor | 0 when closed; 1 when it was not open; 0 for NULL | autodoc; ABI test |
| Alice's chip ID 0x23 | PAL | ABI test |
| WaitBOVP while the beam wraps into the next frame | Returns | ours |
| GfxBase's NormalDisplayRows, MicrosPerLine and similar fields | OS 3.2's values | ABI test (GFXBASE) |
| GfxBase's LastChanceMemory, ActiViewCprSemaphore, MonitorListSemaphore and HashTableSemaphore | Each points at a SignalSemaphore of its own, initialised in the display's state. A monitor driver run by LoadMonDrvs takes MonitorListSemaphore; when it was NULL, ObtainSemaphore read low memory as a semaphore owned by another task and the driver waited for ever | NDK `graphics/gfxbase.h` (the fields); test |
| gfopengfx.library opened next to OS 3.2 | Every call works, but no custom chip register is written (`gd_init`'s `own_hw`) | ours; test |

## The tests

`tests/m68k/test_gfx_display.sh` builds the core with m68k-linux-gnu-g++ 13.3 for the 68040 at -O0 and -O2 (until 17 October 2026 also for the 68000 and 68020). It runs each build under qemu-m68k 8.2 on a simulated machine: exec's memory, the custom registers, a copper running the lists four colour clocks per instruction, and an Agnus/Denise/Lisa that shows the bitplanes line by line, in 35 ns (super hires) pixels. The display models OCS, ECS and AGA; lores, hires and super hires; AGA's 1×, 2× and 4× fetches; HAM6, HAM8, EHB, dual playfield, PF2OF, BPLAM and LOCT. The window follows DIWHIGH when it is written. A line fetched against the chips' rules fails the test: more planes than its fetch shows, a DDFSTRT before 0x18, a DDFSTOP off the fetch's cycle, or a pointer or modulo off a wider fetch's width.

`test_gfx_display_model.py` is written separately from the library and does not model the copper. It makes scenes and says, from the documented View and ViewPort rules, what each ViewPort must show. The scenes cover:

- First a sweep on AGA: one ViewPort of hires and of super hires at every depth from 1 to 8, and lores at 8, each plain and laced.
- Then random ones: OCS, ECS and AGA; PAL and NTSC.
- Laced and plain Views, with or without a ViewExtra.
- One to three ViewPorts each of lores, hires, super hires (ECS and AGA), interlace, EHB, HAM, dual playfield, scrolled, or clipped, at any depth the chips show.
- User copper lists, and 32 to 256 colours.

For each ViewPort, it checks the picture after MakeVPort, MrgCop and LoadView, after ScrollVPort, and after ChangeVPBitMap, in each field. Pictures are compared by a hash of each ViewPort's rectangle.

The harness also checks:

- The database, monitors, VideoControl, sprites, the beam and WaitTOF.
- Sprites 16, 32 and 64 pixels wide through a model of the sprite DMA (control words, data words at FMODE's width, HSTART to 35 ns): every pixel where it should be, through GetExtSpriteA and ChangeExtSpriteA, and on a hires ViewPort at an odd x; an attached pair at 64; OCS and ECS refusing them.
- Multiscan, DblNTSC and DblPAL (`monitors31`). The drivers' MonitorSpecs and SpecialMonitors are added to MonitorList as a driver does. Every one of their 56 modes' records is printed as MonitorProbe prints it, and the script compares them line by line with OS 3.2.3's (`display_os323_modes.txt`). The check also covers MakeVPort's registers for OS 3.2.3's six screens, MrgCop's waits through lines 255 and 511, LoadView calling `do_monitor` once, and the modes going when the driver goes.
- The fetch per chipset: DimensionInfo's MaxDepth for each kind of mode; MakeVPort's answer for lores, hires and super hires at depths 1 to 8, plain and laced; and the DDFSTRT, DDFSTOP, modulos, BPLCON1 and FMODE it sets. Also SysInfo 4.4's screen (640 super hires pixels, 3 planes: 4×, 0x38-0x88), a scrolled 8-plane hires screen at 4×, lores on 32 bits at 2×, a lores screen at the left edge scrolled back to 1×, bitmaps off a wider fetch's width, and FMODE taking a 32-pixel sprite's width in a shown View.
- Running out of memory, and all memory given back.
- The side-by-side mode.

After that, the script:

- Runs `check_regs.py` on `display_lvo.c`.
- Compiles `display_lvo.c` as C.
- Runs nm checks: the core calls nothing but libgcc, and there is no .data, .bss or C++ runtime.

Result (m68k-linux-gnu-g++ 13.3.0, qemu-m68k 8.2.2, seed 20261013, 120 scenes, 3 November 2026): every build shows the same 855 ViewPort pictures as the model, with 3923 checks and 0 failed, and the 56 records are OS 3.2.3's. Seed 7 (300 scenes) also passes.

`tests/ScreenDepthProbe.c` runs on an Amiga with any ROM. It prints the MaxDepth of lores, hires and super hires, whether SysInfo 4.4's screen and its neighbours open, and a table of every resolution and depth from 1 to 8, plain and laced. With MODES and DEPTH it holds one screen on show, with a bar for each colour, for a screenshot. `library/display/build-amiga.sh` builds it.

`DisplayABITest` has not yet been run on OS 3.2. It needs the stove and a boot with DEVS:Monitors empty, because GFOpenGfx has only the built-in PAL and NTSC monitors.

## Not yet

- A 16-pixel sprite's data shown while the width is 32 or 64 (another program's wide sprite) is read at that width: graphics.library does not convert it.
- Euro72, Euro36, Super72 and A2024: their drivers' modes are in the database (see "Monitor drivers"), but OpenScreen, BestModeIDA and CoerceMode only know the built-in, Multiscan, DblNTSC and DblPAL monitors.
- The test's machine shows 15 kHz frames only. The 31 kHz modes are checked against OS 3.2.3's registers, not pixel by pixel.
- `do_monitor` is called as a C function with the MonitorSpec on the stack (`SpecialMonitor`'s declared type). OS 3.2.3's own call has not been seen.
- DTAG_VEC of the built-in modes.
- -738: no OS 3.2.3 driver calls it, so what it does is unmeasured; it answers 0.
- `xln_Init` of the other extended node types: OS 3.2 sets one in a SpecialMonitor too. Nothing has been seen to call it, and ours is NULL.
- `gfx_hook_vsprites`: SprIns from gels' VSprites is still the stub.
- copinit's wait14 is a NOOP, not a wait at line 10.
- `DisplayABITest` has not yet been run against OS 3.2.
