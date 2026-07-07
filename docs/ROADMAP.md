# RIFT: The Skyfell Engine — Implementation Roadmap

## Overview

**Project**: RIFT: The Skyfell Engine (`skyfell`) — a real Super Nintendo game (`.sfc` ROM) built with PVSnesLib, playable in emulators and on real hardware via flashcart.

**Vision**: A portal-gun puzzle-platformer/Metroidvania where portals conserve momentum and — in special Mode 7 chambers — redefine gravity, rotating the whole room around the player. Success = a ROM a stranger can download, run in an emulator, and have fun with for 30+ minutes.

**Current Phase**: Phase 0 (not started — planning complete 2026-07-07)

**Milestone map**:

| Milestone | After phase | Artifact |
|---|---|---|
| **A — "Rift Demo"** (go/no-go) | 3.5 | 1 zone (7 rooms + 1 gravity chamber), 2 enemies, 1 music track, title card. Proves the whole fantasy. |
| **B — "Vertical Kingdom"** | 5 | 3 zones, save system, map, ability gates, Keeper of the Core boss |
| **C — v1.0** | 7 | 5 zones, full audio, polish, real-hardware validated |

Rough sizing: Milestone A ≈ 8–9 focused sessions; v1.0 ≈ 18–22. Reassess at every milestone — **Milestone A is the explicit go/no-go for everything after it.**

Toolchain facts below were verified against live sources on 2026-07-07: PVSnesLib 4.5.0 (released 2025-12-28, MIT, `pvsneslib_450_64b_windows.zip`), bundled tools `gfx4snes`, `gfx2snes`, `tmx2snes`, `smconv`, `snesbrr`, `816-opt`; emulator = MesenCE (nesdev-org/MesenCE — the maintained successor; SourMesen/Mesen2 archived at v2.1.1 mid-2025) with `testrunner` + Lua CLI confirmed in Mesen2 source (`UI/Utilities/CommandLineHelper.cs`).

---

## Phase 0: Toolchain Bring-Up & Walking Skeleton

**Status**: 📋 Planned

**Goal**: A clean checkout builds a booting ROM and runs one automated emulator test, on this Windows machine, with one command each.

### Included Features

- [ ] Install PVSnesLib **4.5.0 pinned** (`pvsneslib_450_64b_windows.zip`) outside the repo; set `PVSNESLIB_HOME` (verify exact env var name against the release's install notes)
- [ ] Install MesenCE (latest release) outside the repo; record exact version in `docs/CONTINUATION.md`
- [ ] `Makefile`: `make` → `build/skyfell.sfc` (LoROM, FastROM flagged, header name `RIFT SKYFELL ENGINE`), `make test` → TEST build + run Lua suite, `make run` → launch MesenCE with the ROM, `make clean`
- [ ] Hello-world ROM: Mode 1, backdrop color cycle + "SKYFELL" text, NMI handler with vblank queue stub
- [ ] Test harness: `tools/run_tests.py` + `tests/lua/lib/harness.lua`; first test `test_boot.lua` asserts debug-RAM magic `0x51FE` at `$7E1F00` and takes a screenshot; **verify `--testrunner` flag behavior in MesenCE** (confirmed present in Mesen2 source; fallback = windowed launch, Lua `emu.stop(code)` still gates)
- [ ] Spike: run `gfx4snes` on one 16-color PNG and `tmx2snes` on a trivial Tiled map — confirm both produce linkable output and whether tmx2snes carries custom tile properties (decides R4 fallback)
- [ ] CI: GitHub Actions **build-only** job (Linux zip); test job deferred (R5)

### Explicit Exclusions

- No gameplay, no player sprite, no audio. Resist starting the game before the harness exists.
- No custom map converter yet — only if the tmx2snes spike fails.

### Success Criteria

- [ ] `make && make test` green on a clean checkout (documented in README + CLAUDE.md)
- [ ] `artifacts/test_boot/boot.png` shows the hello screen
- [ ] CONTINUATION.md records exact tool versions + install paths

### Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| R7: Windows `make`/env friction | Low | pvsneslib Windows zip bundles its toolchain; document every env var; nothing vendored into the repo |
| R5: testrunner needs a display on CI | Med | CI builds only for now; tests gate locally. Revisit xvfb on Linux later. |

---

## Phase 1: Platformer Core (Mode 1)

**Status**: 📋 Planned

**Goal**: A tight-feeling character running and jumping around one scrollable room. This is the foundation everything sits on — get the *feel* right here.

### Included Features

- [ ] Metatile room format: 16×16 metatiles (4 tiles + collision byte + material byte), loaded from converted Tiled maps; one test room ~64×32 metatiles
- [ ] Fixed-point physics: positions s32 16.8, velocities s16 8.8, **no floats anywhere** (INV-ENG-001); all tunables in `src/game/tuning.h`
- [ ] Player FSM: idle/run/jump/fall/land; coyote time (4f) + jump buffer (4f)
- [ ] AABB-vs-metatile collision (swept per-axis)
- [ ] Camera: deadzone follow + room-edge clamp; BG column/row streaming through the vblank queue
- [ ] Meta-sprite system + OAM shadow; player 16×32 (2×16×16), current frame DMA-streamed per vblank
- [ ] BG3 HUD stub (2bpp)
- [ ] Reference: pvsneslib `snes-examples/games/likemario` (verified to exist) for structure, not copy-paste

### Explicit Exclusions

- No portals, no enemies, no damage, no room transitions. One room only.

### Success Criteria

- [ ] `test_walk.lua`: scripted right-walk reaches expected x within tolerance; wall stops movement exactly at boundary
- [ ] `test_jump.lua`: apex height deterministic across runs; lock the measured value as a **golden number** and assert it thereafter
- [ ] `test_replay.lua`: identical input script twice ⇒ bit-identical debug-RAM state (INV-ENG-002 established here, kept green forever)
- [ ] 60fps sustained: Mesen debugger shows main loop finishing well before vblank (record measured scanline in CONTINUATION.md)

---

## Phase 2: The Rift Gun

**Status**: 📋 Planned

**Goal**: Two linked portals with honest momentum. The moment-to-moment toy must be fun in one room before any level design happens.

### Included Features

- [ ] Aiming: D-pad direction + R hold = 8-way aim lock; Y fires current color, X toggles blue/gold, Select recalls both portals
- [ ] Portal shot: 4px/f straight projectile; places a portal on the first **brass**-material surface with 3-metatile clearance; rejects occupied/undersized targets with a fizzle
- [ ] Portal state: max 1 blue + 1 gold globally (INV-ENG-004); 48px openings on any axis-aligned surface (4 orientations only, D-003); rendered as animated BG tiles (palette-cycled shimmer) + 2 sprite caps
- [ ] Teleport: player/objects crossing the plane transform position+velocity via the 16-entry orientation LUT (INV-ENG-003); 8-frame re-entry cooldown; camera snap with 4-frame ease; speed cap 6px/f
- [ ] Crate entity: pushable, grabbable (A), teleportable, lands on switches
- [ ] Sentry enemy: fixed turret, straight shots; shots teleport through portals — reflect them back to kill it
- [ ] Entity pool (16 slots, function-pointer update table)

### Explicit Exclusions

- No gravity changes (that's Phase 3, chambers only). No glass/magnet materials yet. No sound.

### Success Criteria

- [ ] `test_fling.lua`: enter floor portal falling at v ⇒ exit wall portal at same |v| (±1 subpixel), sails the gap the design predicts
- [ ] `test_portal_rules.lua`: placement rejected on non-brass, undersized, and entity-occupied surfaces; third shot of same color moves the old portal
- [ ] `test_sentry.lua`: scripted portal pair routes a sentry shot back into it; sentry dies
- [ ] Replay test still bit-identical; frame budget still green

---

## Phase 3: Gravity Chambers (Mode 7)

**Status**: 📋 Planned

**Goal**: The marquee mechanic — step through a wall portal and the whole room rotates around you while gravity follows. This phase is the point of the game; if it doesn't sing, Milestone A decides what changes.

### Included Features

- [ ] Chamber room type: Mode 7 BG (128×128 map, 8bpp tiles, VRAM bytes $0000–$7FFF interleaved), dedicated VRAM/CGRAM layout swapped behind a force-blank door transition; HUD switches to sprites (Mode 7 has no BG3)
- [ ] Rotation math: 256-entry sin/cos LUT (8.8); M7A–D matrix + M7X/M7Y pivot; world data stays fixed, a screen-transform (90° swaps) maps world→screen for all sprites (D-005)
- [ ] Gravity rule: exiting a chamber portal sets **gravity = −(exit surface outward normal)**; transition = physics frozen, θ eased over ~40 frames, player pinned upright at screen center, then play resumes in the new gravity frame
- [ ] Physics in gravity-frame: input/movement axes remapped by current orientation; collision queries unchanged (world-space)
- [ ] Gale Drone enemy: floating, **radially symmetric sprite** so rotation never shows a wrong angle (hardware: sprites cannot rotate — INV-HW-004)
- [ ] One complete puzzle chamber: requires two reorientations + a crate to open the exit
- [ ] Chamber palette discipline: tiles use colors 0–127, sprites 128–255 (INV-HW-006)

### Explicit Exclusions

- No boss. No mid-rotation player control. Chambers are single-screen-scale only (no scrolling Mode 7 rooms).

### Success Criteria

- [ ] `test_gravity_cycle.lua`: scripted 4-rotation loop returns player to start cell with gravity=down and **bit-identical** debug state; screenshot at each 90° checked into artifacts
- [ ] `test_chamber_puzzle.lua`: scripted full solve reaches the exit
- [ ] Rotation animation holds 60fps (matrix writes are 8 register writes/frame — verify no vblank overrun)
- [ ] A human (Jeremy) plays it and the spin feels *good* — this gate is subjective on purpose

### Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| R2: VRAM/CGRAM swap or 8bpp palette pressure blows up | Med | Spike the chamber render first, puzzle logic second; force-blank transitions hide all loading |
| Rotation feels disorienting instead of cool | High | Tunables: spin duration/easing in tuning.h; fallback design = instant 90° cut + player tumble animation (still fun, zero Mode 7 risk) |

---

## Phase 3.5: Demo Dressing → **Milestone A**

**Status**: 📋 Planned

**Goal**: Wrap Phases 1–3 into a 15-minute ROM someone would actually enjoy: Zone 1 "The Gantries" (7 rooms) + the chamber, both enemies, title card, and first audio.

### Included Features

- [ ] Zone 1 layout: 7 Mode 1 rooms teaching move → portal → fling → crate → sentry, gated door into the chamber; door-based transitions (fade + force-blank reload)
- [ ] Title screen (logo, START), death/respawn at room entry, end-of-demo card
- [ ] Audio spike end-to-end: 1 SNESMOD track through `smconv` + 6 SFX (jump, fire, portal-open, teleport, land, death) — proves R3 before Phase 6 composes more; SFX reserve 2 of the 8 DSP voices
- [ ] Tile-budget audit: Zone 1 sheet ≤ 384 BG tiles, documented reuse map in `assets/README.md`
- [ ] Playtest build handed to Jeremy + feedback logged in DECISIONS.md

### Success Criteria

- [ ] Full suite green; a stranger can play title → demo end without instructions
- [ ] **Milestone A review recorded in DECISIONS.md: continue / re-scope / stop**

---

## Phase 4: Metroidvania Systems

**Status**: 📋 Planned

**Goal**: The connective tissue that turns rooms into a world.

### Included Features

- [ ] SRAM save: 3 slots, versioned + checksummed, corrupt slot degrades to empty (INV-ENG-006); file-select screen
- [ ] Pause map (BG3): visited-room grid, current position, save points
- [ ] Health/damage/knockback/i-frames; respawn at last save
- [ ] Ability gates: **glass portals** upgrade (portals placeable on glass; shots pass through glass), gating Zone 2; item-pickup ceremony
- [ ] World graph: room exits/doors as data; backtracking shortcuts
- [ ] Room streaming polish: adjacent-room preload if trivially affordable, else keep door loads

### Explicit Exclusions

- Magnet-plates (moving portals) deferred to Phase 5 — highest-risk mechanic, wants boss-free testing space first.

### Success Criteria

- [ ] `test_save_roundtrip.lua`: save → reset → load restores position/abilities/map bits exactly; corrupted-SRAM test falls back cleanly
- [ ] `test_gate.lua`: glass wall rejects portals before upgrade, accepts after

---

## Phase 5: Content Build-Out & The Keeper

**Status**: 📋 Planned

**Goal**: Zones 2–3, the magnet-plate mechanic, and the first real boss.

### Included Features

- [ ] Zone 2 "Cistern of Winds" (water/steam momentum puzzles), Zone 3 "The Foundry" (heat + magnet-plates: portals that slide on rails while you're mid-flight)
- [ ] 2 new enemies (Brass Wasp flyer, Custodian miniboss)
- [ ] **Keeper of the Core** boss: chamber fight; boss is a radially symmetric ring-machine (sprite-rotation-proof) that re-anchors to a new surface each phase, forcing gravity re-solves under fire
- [ ] Tinker's Roost hub NPC + story beats (saboteur mystery drip)
- [ ] ROM pressure check; add RLE/LZ compression for maps/tiles only if needed

### Success Criteria

- [ ] Boss beatable by scripted replay (golden) and by hand; all zone puzzles solvable via scripted tests
- [ ] ROM ≤ 1MB, VRAM/ARAM budgets green per INVARIANTS

---

## Phase 6: Audio Suite

**Status**: 📋 Planned

**Goal**: Full soundtrack + mixed SFX inside the APU's real limits (64KB ARAM, 8 voices, BRR samples).

### Included Features

- [ ] 5–6 tracks (title, 3 zone themes, chamber ambient, boss) as Impulse Tracker modules through `smconv`; per-track ARAM budget asserted at build time (INV-AUD-001)
- [ ] Composition pipeline decision (D-open): hand-authored in OpenMPT vs. `tools/trk.py` score-DSL → .it writer; Phase 3.5's spike informs which
- [ ] Full SFX bank via `snesbrr`; voice-stealing priority table
- [ ] Mix pass on real output (record from MesenCE, listen)

### Success Criteria

- [ ] Soundbank builds green with headroom; no voice starvation during boss chaos (scripted worst-case test)

---

## Phase 7: Polish & Release (v1.0)

**Status**: 📋 Planned

**Goal**: Ship it.

### Included Features

- [ ] Zones 4–5 ("Glasshouse Gardens", "The Core Vault"), ending sequence
- [ ] HDMA niceties: sky gradients, parallax (pvsneslib `graphics/Effects` examples verified: HDMAGradient, ParallaxScrolling)
- [ ] FastROM timing audit; NTSC-only declared (PAL = future)
- [ ] Real-hardware validation on FXPAK/flashcart (borrow/buy — user's call), plus accuracy pass in ares as a second emulator
- [ ] Difficulty/playtest pass; photosensitivity check (teleport flash ≤ 2 frames, no >3Hz strobes)
- [ ] Release: tagged ROM + README screenshots; distribution decision (itch.io page) is **Jeremy's call — outward-facing**

### Success Criteria

- [ ] Full test suite + golden replays green on the release commit; ROM checksum recorded
- [ ] Plays start-to-credits on real hardware or highest-accuracy emulator without crash

---

## Future Considerations (Beyond v1.0)

- **2P split-screen race mode** (the original pitch's chaos mode) — big; wants its own plan
- PAL support; MSU-1 audio edition; speedrun timer + leaderboard ROM hack hooks
- 45° portal surfaces (rejected for v1 in D-003 — revisit only with a working prototype)

---

## Decision Log

Lives in [docs/DECISIONS.md](./DECISIONS.md) (D-001…D-009 seeded at planning time).

## Checkpoints & Review

Roadmap is reassessed at each milestone (A/B/C). Phase completion requires: suite green on clean checkout, CONTINUATION.md updated, checkpoint summary appended to DECISIONS.md.

---

**Last Updated**: 2026-07-07 (planning session — nothing built yet)
**Next Review**: end of Phase 0
