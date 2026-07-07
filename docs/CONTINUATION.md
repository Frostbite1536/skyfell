# CONTINUATION — read this first, every session

**Project**: RIFT: The Skyfell Engine — SNES ROM. Docs are law: [ROADMAP](./ROADMAP.md) (build order) · [GDD](./GDD.md) (design) · [ARCHITECTURE](./ARCHITECTURE.md) · [INVARIANTS](./INVARIANTS.md) · [DECISIONS](./DECISIONS.md).

## Current state (2026-07-07)

- **Phase**: 0 not started. Planning session complete; repo scaffolded, docs only. **No code, no Makefile, no toolchain installed yet.**
- **Git**: local repo only — **no GitHub remote yet** (Jeremy to decide public/private + name).
- **Test baseline**: none (no suite exists). First baseline gets recorded here at end of Phase 0.
- **Open user decisions**: repo name ok? (`skyfell`) · GitHub remote? · license (D-009).

## Environment facts (verified 2026-07-07)

- Windows 10, primary shell PowerShell; Git Bash available. Repos live in `C:\Users\LCM\Github\`.
- PVSnesLib **4.5.0** = the pinned toolchain. Release asset: `pvsneslib_450_64b_windows.zip` from github.com/alekmaul/pvsneslib (released 2025-12-28). Install **outside** the repo; set `PVSNESLIB_HOME` (verify exact var name in the zip's install notes — flagged in DECISIONS checkpoint).
- Emulator: **MesenCE** from github.com/nesdev-org/MesenCE (Mesen2 line; SourMesen/Mesen2 is archived). `--testrunner rom.sfc script.lua [timeout=N]` + Lua confirmed in Mesen2 source.
- SafeVibeCoding reference lives at `C:\Users\LCM\Github\SafeVibeCoding` (structure source: `INCORPORATE.md`).

## Next actions (in order)

1. Phase 0 per ROADMAP: install pvsneslib 4.5.0 + MesenCE, record exact versions/paths **here**
2. Makefile + hello-world ROM (LoROM/FastROM, header `RIFT SKYFELL ENGINE`)
3. `tools/run_tests.py` + `tests/lua/lib/harness.lua` + `test_boot.lua`; prove testrunner invocation on this machine
4. gfx4snes + tmx2snes spikes (decides R4 fallback)
5. Update this file + DECISIONS checkpoint; only then start Phase 1

## Session gotchas learned

- (2026-07-07) GitHub code search API returned empty for repo-scoped queries via `gh api search/code` — fetch known file paths directly instead.
- Keep tool zips out of the repo; `.gitignore` already covers `build/`, `artifacts/`, `tools/bin/`.

## Handoff rules

End every session by updating: current phase/state, test counts (from the gate's own output, not memory), exact next actions, and anything learned that isn't in the docs. This file must stand alone — the next session reads it, not your chat history.
