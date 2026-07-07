# RIFT: The Skyfell Engine

A **Super Nintendo game** — a real `.sfc` ROM, not a "retro-style" PC game. Portal-gun puzzle-platformer/Metroidvania: your two linked portals conserve momentum everywhere, and inside the broken Engine's gyre chambers they redefine gravity itself — step through a portal onto a wall and the whole room rotates around you (hardware Mode 7) while you stay upright.

You are Wren, an apprentice riftwright descending through the shattered Skyfell Engine to relight it — and to learn who broke it.

**Status**: 🚧 Planning complete (2026-07-07) — Phase 0 (toolchain bring-up) not started. See [docs/ROADMAP.md](docs/ROADMAP.md).

## Quick start

> Available after Phase 0 lands. The contract these commands will honor:

```
make        # build build/skyfell.sfc  (PVSnesLib 4.5.0, LoROM/FastROM)
make test   # TEST ROM + automated emulator suite (MesenCE --testrunner + Lua)
make run    # launch the ROM in MesenCE
```

Prereqs (installed outside the repo, per [docs/CONTINUATION.md](docs/CONTINUATION.md)): [PVSnesLib 4.5.0](https://github.com/alekmaul/pvsneslib) (`PVSNESLIB_HOME`), [MesenCE](https://github.com/nesdev-org/MesenCE).

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
