# CONTINUATION — read this first, every session

**Project**: RIFT: The Skyfell Engine — SNES ROM. Docs are law: [ROADMAP](./ROADMAP.md) (build order) · [GDD](./GDD.md) (design) · [ARCHITECTURE](./ARCHITECTURE.md) · [INVARIANTS](./INVARIANTS.md) · [DECISIONS](./DECISIONS.md) · **[JEREMY-INBOX](./JEREMY-INBOX.md) (decisions parked for Jeremy — read it, don't build past it)**.

## Current state (2026-07-12)

- **Phase 1 COMPLETE** (platformer core) — cold-clean gate **6/6**: `test_boot`, `test_room`, `test_vblank`, `test_walk`, `test_jump`, `test_replay`. Wren runs/jumps through the 64×32-metatile gantry hall with streaming camera at a measured 0 lag frames.
- **Golden numbers frozen from the ROM** (2026-07-12, GDD-aligned tuning — see DECISIONS checkpoint): walk60=154568 (gantry runway) · wall=4096 · apex=97888 (35.6px rise) · land=107008. INV-ENG-002 (test_replay) is live — green forever from here.
- **Level-design law learned**: max climbable ledge at the GDD apex = 2 metatiles; room01's rung ladder is spaced accordingly (rows 26/24/22/20/18). A jump-reachable platform 3 metatiles up is unreachable — check every authored room against the apex.
- **Engine facts measured on THIS ROM**: vblank drain starts at scanline **v=230** (dbg_vbl_v probe); budget = (261−v)×160 bytes, VQ_ENTRY_TAX 640 defers at the 8th tiny entry. Re-derive if the ISR grows (SNESMOD lands Phase 3.5).
- **Debug block**: `$7EFF00`, magic `0x51FE` (D-010); **next append at +48**; mailbox verbs 1 RESEED / 2 VQ_STRESS / 3 VQ_BUDGET / 4 POS_SET (tests/README.md is the contract). VRAM words `0x7C00+` are test scratch.
- **Room pipeline (D-011 + D-012)**: `assets/maps/*.txt` (ASCII, 64×32 metatiles) → mkmaps → `.tmj` → tmx2snes → roomglue bakes words + INV-ENG-005 checksum. Tileset + player sprite are deterministic generated art (`tools/art/mktiles.py`, `mkobj.py`).
- **Git**: remote `https://github.com/Frostbite1536/skyfell`; Phase 0 = `fcbf047`/`3c31f63` (CI green, artifact verified). Phase 1 units: `ef8fa7d` (engine core), `8761feb` (room), `5bb65ab` (player) + gate/docs commit. **Overnight run 2026-07-11→12: Jeremy authorized full autonomy** — commit per unit, push per phase, route open calls to JEREMY-INBOX.md.
- **Open user decisions**: license (D-009) + the feel gates listed in JEREMY-INBOX.md.

## Environment facts (verified by running)

- **PVSnesLib 4.5.0** at `C:/Users/LCM/snesdev/pvsneslib` — `PVSNESLIB_HOME` must be **forward-slash `C:/...`**, exported inline (not in any shell profile).
- **The gate** (make = MSYS2 4.4.1; python NOT on MSYS2 path):
  `C:\msys64\usr\bin\bash.exe -lc "export PVSNESLIB_HOME=C:/Users/LCM/snesdev/pvsneslib; make -C /c/Users/LCM/Github/skyfell test PYTHON=C:/Python314/python.exe"`
- **MesenCE 2.2.1** at `C:/Users/LCM/snesdev/mesence/Mesen.exe` — DOTNET_ROOT injected by run_tests.py; portable settings.json beside the exe; `emu.setInput(table, subport, PORT)` port is the 3rd arg.
- **pvsneslib gotchas (Phase 0/1)**: `nmiSet()` OVERRIDES the default handler (the boot console needed `consoleVblank()` chained — console now retired); `consoleUpdate()` DMAs to hardcoded $0800 — never use; CGRAM port is `*CGRAM_PALETTE`; force-blank time counts into `lag_frame_counter` — zero it after loads/warps so dbg_lag==0 means LIVE 60fps.
- **tcc816 care taken in Phase 1** (inherited from prophet's lessons): parallel arrays never struct arrays in hot paths; no signed right shift (negate-shift-negate in player.c); ROM data = direct extern label indexing, never stored pointers; statics reset explicitly.
- The deeper toolchain lore lives in `../prophet/docs/CONTINUATION.md` ("Environment facts" + "Hardware/toolchain lessons") — consult before each new phase.

## Next actions (in order) — Phase 2: The Rift Gun

1. **Entity pool first** (16 slots, function-pointer table per ARCHITECTURE) — crate + sentry + portal shots all ride it.
2. **Aiming + portal shot**: D-pad + R 8-way aim lock; Y fires, X toggles blue/gold, Select recalls; 4px/f projectile; placement rules (first brass surface, 3-metatile clearance, fizzle on reject). Brass material already flows through the pipeline (attr high byte 1; the brass tower + floor pad in room01 are the targets).
3. **Portal state + render**: 1 blue + 1 gold max (INV-ENG-004), 48px openings, 4 orientations (D-003); animated BG tiles + 2 sprite caps.
4. **Teleport**: the 16-entry orientation transform LUT (INV-ENG-003) rewriting pos+vel; 8f re-entry cooldown; camera snap w/ 4f ease; speed cap 6px/f. **Momentum conservation is the game** — test_fling gates |v| within ±1 subpixel.
5. Tests: `test_fling`, `test_portal_rules`, `test_sentry`; test_replay must stay green untouched.
6. dbg appends start at +48; dbg_pblue/+19, dbg_pgold/+20, dbg_entn/+21 come live.

## Session gotchas learned

- (2026-07-12) A green suite hid a real stall once already: test_room's hold-right script parked Wren against the 32px step block and the camera never crossed 500 — the trace probe (5-frame px/vx dump into an artifact file) found it in one run. Probe pattern: temp `test_zzz*.lua` writing to OUTDIR, delete before commit.
- (2026-07-12) Goldens are HARVESTED, never computed: temp probe test writes goldens.txt from the ROM, values get frozen into the real test, probe deleted. And they double as a staleness canary: a tuning.h edit that produces IDENTICAL goldens = a stale object linked in (snes_rules has no header deps; the Makefile's blanket header rule now guards this).
- (2026-07-11) `make -C <dir>` is the reliable MSYS2 form (embedded `cd` may not take).
- (2026-07-07) pvsneslib lib source is NOT in the install zip — ground-truth via `gh api repos/alekmaul/pvsneslib/contents/...?ref=4.5.0`.

## Handoff rules

End every session by updating: current phase/state, test counts (from the gate's own output, not memory), exact next actions, and anything learned that isn't in the docs. This file must stand alone — the next session reads it, not your chat history.
