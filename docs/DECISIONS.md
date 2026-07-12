# Decision Log — RIFT: The Skyfell Engine

Settled questions with rationale, so future sessions don't re-litigate them. Newest first. Format: date, decision, context, alternatives, rationale.

---

## D-012 — Room runtime format: per-8×8-tile attributes; metatiles are an authoring convention; ASCII grid is the authoring source
**2026-07-11** · Context: Phase 1 room loader design (the "metatile semantics" D-011 deferred). Decision: the runtime consumes tmx2snes outputs near-verbatim — the room is a 128×64 grid of SNES map words (tmx2snes `.m16` with `.t16` palette/priority baked in at build time by `tools/roomglue.py`, which also emits the INV-ENG-005 checksum), and collision/material is a per-tileset-tile u16 (`.b16`: low byte collision class, high byte material) looked up through the map word. 16×16 "metatiles" survive as the authoring grid: rooms are authored as a 64×32 ASCII grid (`assets/maps/*.txt`, one char = one metatile = a 2×2 tile quad from `tools/tilesdef.py`), converted to Tiled `.tmj` by `tools/mkmaps.py`, then through tmx2snes (D-011 honored — it stays the sole map converter). Rationale: (a) rendering streams map words straight to VRAM with zero metatile expansion; (b) collision at 8×8 granularity is finer, not coarser, than the planned 16×16 and the query is two array reads; (c) a 64×32-metatile room costs 16.5KB ROM uncompressed — at 512KB with a Phase 5 pressure check, per-room cost is a non-issue; (d) ASCII grids diff cleanly in git and need no GUI overnight — Tiled interchange stays available since `.tmj` is the pipeline's native input (a future Tiled-authored map drops in unchanged). Alternatives: literal metatile-def + grid format (rejected: adds a runtime expansion layer and a custom converter for no measured saving); hand-written `.tmx` in Tiled (deferred to Jeremy's art passes, not a machine workflow).

## D-011 — Map pipeline: tmx2snes adopted; R4 custom-converter fallback NOT triggered
**2026-07-11** · Context: ROADMAP Phase 0 spike. Verified by running tmx2snes 4.5.0 on the pvsneslib likemario map: outputs (`.m16` map, `.b16` tile attributes, `.o16` objects, `.t16` palette/priority) are **byte-identical to the example's committed goldens** (deterministic), and custom tile properties DO carry — each tile's Tiled `attribute` property is a free u16 written verbatim (little-endian) to `.b16`, enough for skyfell's collision byte + material byte; `palette`/`priority` land in `.t16`. Caveats: the converter reads Tiled's **JSON export (`.tmj`)**, not raw `.tmx` (author `.tmx` with an `editorsettings` auto-export, commit both); it's 8×8-tile oriented with named layers — 16×16 metatile semantics are the room loader's job (Phase 1 design). Alternatives: custom Python converter (R4 fallback) — not needed; revisit only if metatile authoring in Tiled proves painful.

## D-010 — Debug block re-pinned to `$7EFF00` (was `$7E1F00`); magic stays `0x51FE`
**2026-07-11** · Context: sibling repo `../prophet` proved during its Phase 0 bring-up that `$7E1F00` is unusable on PVSnesLib 4.5.0 — `crt0_snes.asm` (line 244) sets the stack to `$1FFF` growing **down** through the `$1F00` page, colliding with any block there (`prophet/dbg.asm:10-12`, `prophet/tests/README.md:19`). Skyfell had pinned `$7E1F00` at planning time, before any code existed; no test or offset ever shipped against it, so this is a paper-only move, not an INV-TEST-001 violation. Decision: block lives at `$7E:FF00` (top page of WRAM bank `$7E`), declared `.RAMSECTION ".dbg" BANK 126 SLOT 2 ORGA $FF00 FORCE` — BANK must be 126 (= `$7E`), not 0, because tcc816 addresses the labels with absolute-long (`sta.l`); `FORCE` makes wlalink fail loudly on any collision. Alternatives: another low page (rejected — anything under `$2000` is stack/DP territory and low WRAM is mirrored, inviting aliasing confusion). Jeremy approved 2026-07-11.

## D-009 — License: OPEN (Jeremy's call)
**2026-07-07** · Code license undecided (MIT vs. all-rights-reserved). Note: pure homebrew — no Nintendo code or assets are used or copied; ROM runs in emulators/flashcarts. Blocks nothing until first public release.

## D-008 — NTSC/60fps only for v1
Context: PAL timing changes vblank budget and music tempo. Alternatives: dual-region. Rationale: one timing target keeps determinism tests simple; PAL is a v1.x consideration.

## D-007 — Repo structure per SafeVibeCoding; N/A tiers skipped with rationale
Context: user directive to follow `SafeVibeCoding` (INCORPORATE.md file-placement tree). Skipped: `.env.example` / secrets hygiene (offline cartridge — no secrets exist), `THREAT_MODEL.md` (no user data, no network, no auth), smart-contract items (N/A). Everything else adopted: CLAUDE.md, README, REVIEW.md, docs/{ARCHITECTURE,INVARIANTS,ROADMAP,DECISIONS}, prompts/, tests-that-run (Phase 0 makes this real), CI (build-only at first), multi-agent git rules in CLAUDE.md.

## D-006 — LoROM + FastROM, 512KB, SRAM 8KB
Alternatives: HiROM (no benefit at this size), SlowROM (leaves 25% CPU on the table). Grow to 1MB only under measured ROM pressure (Phase 5 check).

## D-005 — Gravity model: world stays fixed; camera/gravity rotate
Context: the marquee mechanic needs a bug-resistant model. Decision: collision/world data never rotates; gravity is a 4-state vector; physics runs in the "gravity frame" (axis remap); rendering applies the rotation so gravity always points screen-down; **chamber enemies are radially symmetric sprites** because OBJ hardware cannot rotate. Alternatives: rotating the tilemap data (rejected — 4× data or expensive transforms, and a whole class of desync bugs); pre-drawn rotated enemy frames (rejected for v1 — VRAM cost; round enemies are charming anyway).

## D-004 — Mode 7 confined to dedicated chamber rooms
Context: could the whole game be Mode 7? Alternatives: full-time Mode 7 Metroidvania (rejected: single BG layer, 8bpp palette pressure, no BG3 HUD, sprite-rotation mismatch everywhere); no Mode 7 at all (rejected: it's the pitch). Chambers get a dedicated VRAM/CGRAM layout behind force-blank door transitions. Fallback if the spin doesn't feel good (Phase 3 gate): instant 90° cuts + tumble animation.

## D-003 — Portals are axis-aligned only (4 orientations)
Alternatives: 45° surfaces (Portal-authentic, but explodes the transform table, collision, and art). 4×4 orientation pairs → one 16-entry LUT (INV-ENG-003). 45° lives in Future Considerations behind a prototype.

## D-002 — Emulator/test target: MesenCE; accuracy spot-checks on ares
Context: verified 2026-07-07 that SourMesen/Mesen2 is archived (at ~2.1.1, mid-2025) and development continues at **github.com/nesdev-org/MesenCE**. Mesen2-line has SNES support, Lua scripting, and a `testrunner` CLI mode (confirmed in source: `UI/Utilities/CommandLineHelper.cs` — `IsTestRunner`, `.lua` args, `timeout=`). Alternatives: bsnes/ares (accuracy gold but no Lua harness), snes9x (weaker debugging). ares is the pre-release accuracy cross-check; real hardware in Phase 7.

## D-001 — Language: C on PVSnesLib 4.5.0, assembly only where profiling demands
Context: verified 2026-07-07 — PVSnesLib 4.5.0 released 2025-12-28, MIT, Windows 64-bit zip, active; bundles 816-tcc toolchain + `816-opt`, `gfx4snes`, `tmx2snes`, `smconv`, `snesbrr`; ships a platformer example (`snes-examples/games/likemario`). Alternatives: pure 65816 asm via ca65/libSFX (maximum control and cool-factor, but 3–5× slower iteration and a much bigger bug surface for a game this systemic); other C kits (none as maintained). Hot paths (collision, OAM build) drop to asm **after** the Mesen profiler fingers them — not speculatively.

---

## Checkpoint summaries

*(Appended after each phase/milestone: 5 bullets — what changed, new assumptions, new risks.)*

### 2026-07-12 — Phase 1 complete (platformer core), cold-clean 6/6
- Engine core ported from prophet and re-calibrated on THIS ROM (drain-start v=230 measured; VQ_ENTRY_TAX 640 re-derived, not copied); test_vblank proves throughput/defer/overflow back-pressure
- Room pipeline live per D-011+D-012 (ASCII grid → .tmj → tmx2snes → baked words + checksum); camera seam-streaming costs 1 col / 2 row pushes per 8px crossing, zero lag frames
- Wren playable: run/jump/coyote/buffer/variable-height, tuned to the **GDD movement-feel spec** (run 1.5 px/f, gravity 0.1875, terminal 4, apex 35.6px ≈ 2.2 metatiles — slow base movement so 6 px/f portal flings read as dramatic, the momentum pillar). First-cut faster values were replaced pre-push; room01's pit went 3-deep → 2-deep so every jumpable feature clears the real apex (INV-GAME-001).
- **Golden numbers frozen from the ROM 2026-07-12**: walk60=154568 subpx (from x=512 on the gantry runway), wall pin=4096, jump apex=97888, land=107008 (tuning.h freeze: ACC 0x28 MAX 0x180 FRIC 0x20 GRAV 0x30 JUMP -0x3C0 TERM 0x400) — any change = new DECISIONS entry
- Adversarial review (13 agents): 8 latent-hazard claims refuted on reachability (recorded as future-phase watch items in JEREMY-INBOX/notes: OAM X8 bit when rooms lose their 16px borders; dropped-push window advance + scroll/defer pairing if per-frame queue traffic ever grows; win_want clamp underflow when a room < 64×32 tiles exists); 1 CONFIRMED coverage gap fixed — test_room now streams all four directions (right/left columns incl. both 32×32 screens, vertical rows via a warp-fall)
- INV-ENG-002 fence established (test_replay); INV-ENG-005 checksum validates on every room load
- Build lesson: snes_rules tracks NO header deps — a tuning.h edit shipped a stale player.obj (identical goldens exposed it); Makefile now rebuilds every .ps on any project-header change
- New assumption to watch: all rooms 128 tiles wide (stride shift in room.c); BG3 HUD deferred until Phase 2 has something to show

### 2026-07-07 — Planning complete
- Concept locked (GDD), roadmap phased 0→7 with milestone gates A/B/C
- Toolchain verified live: PVSnesLib 4.5.0 / MesenCE / testrunner+Lua
- Assumption: tmx2snes carries custom tile properties (Phase 0 spike verifies; fallback converter planned)
- Assumption: `PVSNESLIB_HOME` is the env var name (verify at install)
- Risk register seeded (R1–R7) in ROADMAP; biggest = scope (R6), mitigated by Milestone A go/no-go
