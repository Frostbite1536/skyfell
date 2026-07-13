# RIFT: The Skyfell Engine — Implementation Roadmap

## Overview

**Project**: RIFT: The Skyfell Engine (`skyfell`) — a real Super Nintendo game (`.sfc` ROM) built with PVSnesLib, playable in emulators and on real hardware via flashcart.

**Vision**: A portal-gun puzzle-platformer/Metroidvania where portals conserve momentum and — in special Mode 7 chambers — redefine gravity, rotating the whole room around the player. Success = a ROM a stranger can download, run in an emulator, and have fun with for 30+ minutes.

**Current Phase**: Phase 3 (gravity chambers) — Phases 0-2 complete (see per-phase status)

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

- [x] Install PVSnesLib **4.5.0 pinned** outside the repo; set `PVSNESLIB_HOME` *(done — shared install at `C:/Users/LCM/snesdev/pvsneslib`, forward-slash path, exported inline per CONTINUATION)*
- [x] Install MesenCE outside the repo; record exact version in `docs/CONTINUATION.md` *(done — MesenCE 2.2.1 at `C:/Users/LCM/snesdev/mesence`)*
- [x] `Makefile`: `make` → `build/skyfell.sfc` (LoROM, FastROM flagged, header name `RIFT SKYFELL ENGINE`), `make test` → TEST build + run Lua suite, `make run` → launch MesenCE with the ROM, `make clean` *(done 2026-07-11, adapted from prophet)*
- [x] Hello-world ROM: Mode 1, backdrop color cycle + "SKYFELL" text, NMI handler with vblank queue stub *(done 2026-07-11 — `src/main.c`, `src/core/vblank.c`)*
- [x] Test harness: `tools/run_tests.py` + `tests/lua/lib/harness.lua`; first test `test_boot.lua` asserts debug-RAM magic `0x51FE` at `$7EFF00` (D-010) and takes a screenshot *(done 2026-07-11 — `--testrunner` proven on this machine; baseline 1/1)*
- [x] Spike: `gfx4snes` *(proven — the font pipeline runs it every build)* and `tmx2snes` on a Tiled map *(done 2026-07-11 — deterministic, custom `attribute` u16 carries to `.b16`; R4 fallback NOT needed; see D-011)*
- [x] CI: GitHub Actions **build-only** job (Linux zip); test job deferred (R5) *(green 2026-07-11 on `fcbf047` — artifact verified: 512KB, correct header)*

### Explicit Exclusions

- No gameplay, no player sprite, no audio. Resist starting the game before the harness exists.
- No custom map converter yet — only if the tmx2snes spike fails.

### Success Criteria

- [x] `make && make test` green on a clean checkout (documented in README + CLAUDE.md) *(1/1, 2026-07-11)*
- [x] `artifacts/test_boot/boot.png` shows the hello screen *(verified by eye — the green gate alone missed a textless screen once; see CONTINUATION gotchas)*
- [x] CONTINUATION.md records exact tool versions + install paths

### Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| R7: Windows `make`/env friction | Low | pvsneslib Windows zip bundles its toolchain; document every env var; nothing vendored into the repo |
| R5: testrunner needs a display on CI | Med | CI builds only for now; tests gate locally. Revisit xvfb on Linux later. |

---

## Phase 1: Platformer Core (Mode 1)

**Status**: ✅ Complete 2026-07-12 (cold-clean gate 6/6)

**Goal**: A tight-feeling character running and jumping around one scrollable room. This is the foundation everything sits on — get the *feel* right here.

### Included Features

- [x] Metatile room format *(as revised by D-012: 16×16 metatiles are the AUTHORING grid — ASCII map → Tiled .tmj → tmx2snes; runtime = per-8×8-tile attribute u16 (collision+material), checksum-validated per INV-ENG-005)*; test room 64×32 metatiles (`assets/maps/room01.txt`)
- [x] Fixed-point physics: positions s32 16.8, velocities s16 8.8, no floats (INV-ENG-001); all tunables in `src/game/tuning.h`
- [x] Player FSM: idle/run/jump/fall (land folded into idle transition); coyote 4f + jump buffer 4f + variable jump height
- [x] AABB-vs-tile collision (swept per-axis; exact wall/floor pins proven by golden tests)
- [x] Camera: deadzone follow + room-edge clamp; BG column/row streaming through the vblank queue (1 col push / 2 row pushes per 8px crossing; zero lag frames measured)
- [x] Meta-sprite: player 16×32 = 2×16×16 OAM shadow (direct stores) — all 8 frames resident in VRAM, so no per-vblank frame streaming needed yet (revisit if OBJ VRAM pressure ever appears)
- [x] ~~BG3 HUD stub~~ deferred to the first phase that actually shows a HUD (Phase 2 aim/portal state) — nothing to display in Phase 1
- [x] Reference likemario: structure consulted; its map engine NOT used (second VRAM writer would break INV-HW-001)

### Explicit Exclusions

- No portals, no enemies, no damage, no room transitions. One room only. *(held)*

### Success Criteria

- [x] `test_walk.lua`: golden walk distance EXACT (154568 subpx after 60 held frames + glide on the gantry runway); wall pins exactly at x=16
- [x] `test_jump.lua`: golden apex EXACT (py 97888 = 35.6px rise, the GDD feel), exact floor re-contact, variable-height tap proven
- [x] `test_replay.lua`: same script twice ⇒ bit-identical px/py/vx/vy/fsm at 3 checkpoints (INV-ENG-002 established, green forever)
- [x] 60fps sustained: dbg_lag==0 asserted across every riding test (walk/jump/replay/room streaming); drain-start scanline measured v=230 (recorded in CONTINUATION.md + vblank.c)

---

## Phase 2: The Rift Gun

**Status**: ✅ Complete 2026-07-12 (cold-clean gate 9/9) — deviations noted per feature

**Goal**: Two linked portals with honest momentum. The moment-to-moment toy must be fun in one room before any level design happens.

### Included Features

- [x] Aiming: D-pad + R = 8-way aim lock w/ reticle sprite; Y fires, X toggles, Select recalls, A grabs/throws
- [x] Portal shot: 4px/f projectile entity; places on the first brass face it crosses (orient from crossing axis), 6-tile strip snaps +-2; rejects non-brass/undersized/occupied (dbg_fizz counts)
- [x] Portal state: 1 blue + 1 gold (INV-ENG-004); 48px openings, 4 orientations (D-003); rendered as BG overlay tiles (cap/body + flips; VRAM-asserted) — shimmer + sprite caps deferred to the Phase 3.5 polish pass
- [x] Teleport: LEADING-EDGE plane crossing per axis, pos+vel through the 16-entry LUT (INV-ENG-003), 8f cooldown, 6px/f cap + 1px/f eject; camera snap-eases at 16px/f (X) / 8px/f (Y)
- [x] Crate entity: pushable, grabbable/throwable (A), teleportable, sleeps at rest (frame budget); switches arrive with Phase 3.5 content
- [x] Sentry: fixed turret w/ activation radius (224px), 2px/f shots that ride portals; the routed shot kills it (test_sentry)
- [x] Entity pool: 16 slots, SoA + compact live/crate lists; if-else dispatch (tcc816 u8-switch trap) instead of function pointers

### Explicit Exclusions

- No gravity changes (that's Phase 3, chambers only). No glass/magnet materials yet. No sound.

### Success Criteria

- [x] `test_fling.lua`: terminal-velocity entry => exit at EXACTLY -|v| (0x400), sails the gap; portal render VRAM-asserted
- [x] `test_portal_rules.lua`: non-brass/undersized/entity-occupied rejected (fizzle counted); refire moves the portal and restores old cells (VRAM-asserted)
- [x] `test_sentry.lua`: tower-east -> pillar-west pair routes the shot into the sentry's back; dies; kill phase lag==0
- [x] Replay bit-identical; frame budget green (dbg_lag==0 asserted per test; the Phase 2 frame-budget program is in DECISIONS — measured with the new dbg_mainv scanline probe)

---

## Phase 3: Gravity Chambers (Mode 7)

**Status**: 📋 Planned

**Goal**: The marquee mechanic — step through a wall portal and the whole room rotates around you while gravity follows. This phase is the point of the game; if it doesn't sing, Milestone A decides what changes.

### Included Features

- [x] Chamber room type: Mode 7 BG (128×128 map, 8bpp tiles, VRAM bytes $0000–$7FFF interleaved), dedicated VRAM/CGRAM layout swapped behind a force-blank door transition; HUD switches to sprites (Mode 7 has no BG3). The LIVE map is a WRAM copy (bank $7F, chamram.asm) — the world mutates (D-015)
- [x] Rotation math: 256-entry sin/cos LUT (8.8); M7A–D matrix + M7X/M7Y pivot; world data stays fixed, a screen-transform (90° swaps) maps world→screen for all sprites (D-005)
- [x] Gravity rule: exiting a chamber portal sets **gravity = −(exit surface outward normal)**; transition = physics frozen, θ eased over 32 frames, player pinned upright at screen center, then play resumes in the new gravity frame
- [x] Physics in gravity-frame: input/movement axes remapped by current orientation; collision queries unchanged (world-space); crates fall along the live gravity vector too
- [x] Gale Drone enemy: floating, **radially symmetric sprite** so rotation never shows a wrong angle (hardware: sprites cannot rotate — INV-HW-004). Leashed floater per D-014; contact damage lands with death/respawn (Phase 3.5)
- [x] One complete puzzle chamber: requires two reorientations + a crate to open the exit (D-015 — the pad is in the ceiling; the crate starts on an unreachable ledge)
- [x] Chamber palette discipline: tiles use colors 0–127, sprites 128–255 (INV-HW-006)
- [x] (Unplanned, from playtest need) The Rift Gun fires INSIDE the chamber: D-013 controls, aim in the screen frame rotated to world velocity

### Explicit Exclusions

- No boss. No mid-rotation player control. Chambers are single-screen-scale only (no scrolling Mode 7 rooms).

### Success Criteria

- [x] `test_gravity_cycle.lua`: scripted 4-rotation loop returns player to start cell with gravity=down and **bit-identical** debug state; screenshot at each 90° checked into artifacts
- [x] `test_chamber_puzzle.lua`: scripted full solve reaches the exit
- [x] Rotation animation holds 60fps (matrix writes are 8 register writes/frame — verify no vblank overrun). dbg_lag==0 asserted across every chamber test; the transit-frame eject and the door-open are cost-spread onto quiet frames (measured by scanline probe)
- [ ] A human (Jeremy) plays it and the spin feels *good* — this gate is subjective on purpose (JEREMY-INBOX)

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
- [x] Title screen (logo, START), death/respawn at room entry, end-of-demo card *(D-018/D-019, 2026-07-13; console-text cards — art logo is a later polish pass)*
- [x] Audio spike end-to-end: 1 SNESMOD track through `smconv` + 6 SFX (jump, fire, portal-open, teleport, land, death) — proves R3 before Phase 6 composes more; SFX reserve 2 of the 8 DSP voices *(D-020, test_audio, 2026-07-13; music on ch 1-6, snesmod SFX voice = ch 8)*
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
- [ ] Composition pipeline decision (D-open): hand-authored in OpenMPT vs. growing `tools/audio/mkit.py`'s IT writer into a score-DSL; the Phase 3.5 spike (D-020) proved the writer end-to-end, so both routes are open
- [ ] Full SFX bank as first-module IT samples through `smconv` (D-020 — snesbrr dropped); voice-stealing priority table
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
