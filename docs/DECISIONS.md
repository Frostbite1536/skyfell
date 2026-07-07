# Decision Log — RIFT: The Skyfell Engine

Settled questions with rationale, so future sessions don't re-litigate them. Newest first. Format: date, decision, context, alternatives, rationale.

---

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

### 2026-07-07 — Planning complete
- Concept locked (GDD), roadmap phased 0→7 with milestone gates A/B/C
- Toolchain verified live: PVSnesLib 4.5.0 / MesenCE / testrunner+Lua
- Assumption: tmx2snes carries custom tile properties (Phase 0 spike verifies; fallback converter planned)
- Assumption: `PVSNESLIB_HOME` is the env var name (verify at install)
- Risk register seeded (R1–R7) in ROADMAP; biggest = scope (R6), mitigated by Milestone A go/no-go
