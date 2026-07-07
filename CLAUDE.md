# CLAUDE.md — RIFT: The Skyfell Engine

SNES puzzle-platformer/Metroidvania (real `.sfc` ROM): portal gun with momentum conservation + Mode 7 gravity-rotation chambers. C on PVSnesLib 4.5.0 (pinned), tested via MesenCE Lua testrunner.

**Start every session by reading `docs/CONTINUATION.md`.** Then consult: `docs/ROADMAP.md` (what to build next), `docs/ARCHITECTURE.md` (how), `docs/INVARIANTS.md` (rules), `docs/GDD.md` (design), `docs/DECISIONS.md` (settled questions — don't re-litigate).

## Commands (exist after Phase 0)

- `make` → `build/skyfell.sfc` · `make test` → TEST ROM + full Lua suite via `tools/run_tests.py` · `make run` → launch in MesenCE · `make clean`
- Toolchain: PVSnesLib 4.5.0 at `PVSNESLIB_HOME` (outside repo). Never bump tool versions silently — that's a DECISIONS.md entry.

## Structure

`src/core` (boot, NMI, vblank queue, math) · `src/game` (player, portal, gravity, entity, room, save) · `src/ui` · `src/audio` · `src/data` (generated only) · `assets/` (source PNG/TMX/IT — see assets/README.md) · `tools/` (converter glue, test runner) · `tests/lua/` (emulator tests) · `build/`, `artifacts/` (gitignored)

## Conventions

- Fixed point only: positions s32 16.8, velocities s16 8.8. **`float`/`double` are banned in `src/`** (INV-ENG-001).
- tcc816 realities: `int` is **16-bit** — be explicit (`u8/s8/u16/s16/u32/s32`); avoid struct copies in hot paths; watch bank-crossing for data pointers.
- All gameplay tunables live in `src/game/tuning.h` — one knob, one place.
- snake_case; one module per system; files ≤ ~500 lines.
- Assets: source of truth is `assets/` (PNG/TMX/IT); `src/data/` is generated — never hand-edit.

## Never do

- Never write PPU registers/VRAM/CGRAM/OAM outside the NMI drain or force-blank — use the vblank queue API (INV-HW-001).
- Never assume a sprite can rotate (INV-HW-004).
- Never claim the ROM works from code inspection or a clean compile. **Done = `make test` output pasted, all green, on a clean build.** For anything visual, attach the test screenshot from `artifacts/`.
- Never commit `build/`, `artifacts/`, ROMs, or toolchain binaries.
- Never move/change existing debug-block offsets at `$7E1F00` — append only (INV-TEST-001).
- Never touch the golden replay values without a DECISIONS.md entry explaining the physics change.

## Verification bar

Tests are emulator-level Lua (`tests/lua/`), run headless via MesenCE `--testrunner`. Determinism is load-bearing: `test_replay.lua` must stay green forever (INV-ENG-002). Record pass/fail counts in `docs/CONTINUATION.md` at session end — read the numbers from the runner's output, not memory.

## Multi-agent git safety

Parallel agents/sessions: work on your own branch or worktree, never tabs-on-main. `git status` before every commit — stage only files this task touched; name-and-leave anything else. No `git pull`/`git checkout` over uncommitted work that isn't yours. Commit/push only when Jeremy asks.
