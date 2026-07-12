# RIFT: The Skyfell Engine

A **Super Nintendo game** — a real `.sfc` ROM, not a "retro-style" PC game. Portal-gun puzzle-platformer/Metroidvania: your two linked portals conserve momentum everywhere, and inside the broken Engine's gyre chambers they redefine gravity itself — step through a portal onto a wall and the whole room rotates around you (hardware Mode 7) while you stay upright.

You are Wren, an apprentice riftwright descending through the shattered Skyfell Engine to relight it — and to learn who broke it.

**Status**: 🚧 Phase 1 (platformer core) complete 2026-07-12 — Wren runs and jumps through a streaming-camera room, physics gated on golden emulator tests (baseline 6/6, replay determinism locked). Next: Phase 2, the Rift Gun. See [docs/ROADMAP.md](docs/ROADMAP.md).

## Quick start

```
make        # build build/skyfell.sfc  (PVSnesLib 4.5.0, LoROM/FastROM)
make test   # TEST ROM + automated emulator suite (MesenCE --testrunner + Lua)
make run    # launch the ROM in MesenCE
make clean
```

Prereqs (installed outside the repo, per [docs/CONTINUATION.md](docs/CONTINUATION.md)): [PVSnesLib 4.5.0](https://github.com/alekmaul/pvsneslib) (`PVSNESLIB_HOME`, forward-slash path), [MesenCE](https://github.com/nesdev-org/MesenCE), Python 3. On Windows, `make` is MSYS2's — the proven one-liner:

```
C:\msys64\usr\bin\bash.exe -lc "export PVSNESLIB_HOME=C:/Users/LCM/snesdev/pvsneslib; make -C /c/Users/LCM/Github/skyfell test PYTHON=C:/Python314/python.exe"
```

## Project structure

```
docs/       GDD, ROADMAP (the implementation plan), ARCHITECTURE, INVARIANTS,
            DECISIONS (settled questions), CONTINUATION (session handoff — read first)
src/        game code (C + targeted 65816 asm) — exists from Phase 0/1
assets/     source art (PNG), maps (Tiled), music (IT) — see assets/README.md
tools/      converter glue + test runner (Python)
tests/      emulator-level Lua tests — see tests/README.md
prompts/    reusable session prompts (engineering / bug-hunt / review)
```

Structure follows [SafeVibeCoding](https://github.com/Frostbite1536/safe-vibe-coding)'s incorporation checklist (see D-007 for adopted/skipped items).

## How it's verified

No claim without a run: every phase gates on scripted emulator tests (input playback + RAM assertions + screenshots) executed by `tools/run_tests.py` through MesenCE's testrunner. Determinism (same inputs ⇒ bit-identical state) is a permanent invariant, so golden replays catch physics regressions.

## Reading order

[GDD](docs/GDD.md) → [ROADMAP](docs/ROADMAP.md) → [ARCHITECTURE](docs/ARCHITECTURE.md) → [INVARIANTS](docs/INVARIANTS.md)

## License

TBD (tracked as D-009). Homebrew: contains no Nintendo code or assets.
