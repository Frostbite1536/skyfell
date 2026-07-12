# CONTINUATION — read this first, every session

**Project**: RIFT: The Skyfell Engine — SNES ROM. Docs are law: [ROADMAP](./ROADMAP.md) (build order) · [GDD](./GDD.md) (design) · [ARCHITECTURE](./ARCHITECTURE.md) · [INVARIANTS](./INVARIANTS.md) · [DECISIONS](./DECISIONS.md).

## Current state (2026-07-11)

- **Phase**: 0 COMPLETE — every ROADMAP Phase 0 box ticked. `make` → `build/skyfell.sfc`; `make test` green on a clean build; hello-world ROM (Mode 1 text + backdrop cycle through the vblank-queue stub — Phase 1 replaces the stub with the full engine ported from `../prophet/src/core/vblank.c`).
- **Test baseline: 1/1** (`test_boot`) — from the runner's own last line, clean build, 2026-07-11. Boot screenshot verified at `artifacts/test_boot/boot.png`.
- **Debug block**: `$7EFF00`, magic `0x51FE` (D-010; layout contract in `tests/README.md`, next append at +26).
- **Map pipeline decided (D-011)**: tmx2snes adopted — deterministic, per-tile `attribute` u16 carries to `.b16` (collision+material fits); input is Tiled's `.tmj` JSON export. R4 fallback closed.
- **Git**: remote `https://github.com/Frostbite1536/skyfell` — Phase 0 committed (`fcbf047`) and pushed; **CI green on that commit**, artifact verified (512KB, `RIFT SKYFELL ENGINE` title, map mode `$30`).
- **Reuse source**: sibling `C:\Users\LCM\Github\prophet` (Phase 8, 39/39 green) — audited 2026-07-11; port map + lessons in the session memory (`prophet-reuse-audit`).
- **Open user decisions**: license (D-009).

## Environment facts (verified by running, 2026-07-11)

- **PVSnesLib 4.5.0** at `C:/Users/LCM/snesdev/pvsneslib` — `PVSNESLIB_HOME` must be **forward-slash `C:/...`**. Not set in any shell profile: export it inline.
- **make = MSYS2 GNU Make 4.4.1**; python is NOT on the MSYS2 path. The proven gate invocation:
  `C:\msys64\usr\bin\bash.exe -lc "export PVSNESLIB_HOME=C:/Users/LCM/snesdev/pvsneslib; make -C /c/Users/LCM/Github/skyfell test PYTHON=C:/Python314/python.exe"`
- **MesenCE 2.2.1** at `C:/Users/LCM/snesdev/mesence/Mesen.exe` — needs user-local .NET 10 (`DOTNET_ROOT`, run_tests.py injects it) and the portable `settings.json` beside the exe (already configured; Port1 = SnesController). `--testrunner` flag proven on this machine this session.
- **pvsneslib gotchas hit in Phase 0**: `nmiSet()` OVERRIDES the default handler — a custom NMI must chain `consoleVblank()` (lib symbol, not in any header) or console text never uploads; `consoleUpdate()` DMAs to a HARDCODED VRAM $0800, ignoring `consoleSetTextMapPtr` — never use it. CGRAM data port is `*CGRAM_PALETTE`, not `REG_CGDATA`.
- The full battle-tested env/toolchain lore (SRAM at `$70:0000`, tcc816 traps, ROM-table rules, 60fps phase-stagger) lives in `../prophet/docs/CONTINUATION.md` "Environment facts" + "Hardware/toolchain lessons" — read before each new phase.

## Next actions (in order) — Phase 1: Platformer Core

1. Port prophet's full engine core first (see the reuse-audit memory): `src/core/vblank.c` (SoA queue + measured NMI byte budget — RE-MEASURE the budget constants on skyfell's ROM, don't copy prophet's) + `vqdata.asm` staging buffer, `src/core/rng.c/h` (swap the "PROP" seed), `src/core/dbgcmd` mailbox pattern for the warp request at +24.
2. Metatile room format (16×16 metatiles: 4 tiles + collision byte + material byte) fed by the tmx2snes pipeline (D-011); one ~64×32 test room in `assets/maps/`.
3. Player FSM + fixed-point physics (s32 16.8 / s16 8.8, tunables in `src/game/tuning.h`), AABB-vs-metatile swept collision.
4. Tests: `test_walk`, `test_jump` (golden apex frozen from the ROM, never a parallel sim), `test_replay` (INV-ENG-002 — green forever from then on).

## Session gotchas learned

- (2026-07-11) See "pvsneslib gotchas" above — consoleVblank chaining cost the session's only debug cycle; the boot screenshot was solid backdrop until then. **Never claim the hello screen from a green test alone: test_boot's asserts passed while the screen was textless. Look at the PNG.**
- (2026-07-11) MSYS2 login shell ignores `cd` embedded some ways; `make -C <dir>` is the reliable form.
- (2026-07-07) GitHub code search API returned empty for repo-scoped queries via `gh api search/code` — fetch known file paths directly instead (that's also how consoles.asm was ground-truthed: pvsneslib lib source is NOT in the install zip; `gh api repos/alekmaul/pvsneslib/contents/...?ref=4.5.0`).
- Keep tool zips out of the repo; `.gitignore` covers `build/`, `artifacts/`, `tools/bin/`, generated data, converter droppings (`*.pic`/`*.pal`/`*.inc`/`*_data.as`/`linkfile`).

## Handoff rules

End every session by updating: current phase/state, test counts (from the gate's own output, not memory), exact next actions, and anything learned that isn't in the docs. This file must stand alone — the next session reads it, not your chat history.
