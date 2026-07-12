# System Invariants & Contracts — RIFT: The Skyfell Engine

Non-negotiable truths. **If a change violates one of these, it is a bug or a product decision — never an implementation detail.** Review before implementing; check during review; verify when debugging. Changes require an entry in the change log below.

---

## Hardware Invariants (the SNES does not negotiate)

### INV-HW-001: VRAM/CGRAM/OAM writes only during blank
**Rule**: Game code never writes PPU memory or registers during active display. All updates go through the vblank queue (drained in NMI) or happen under force-blank.
**Rationale**: Mid-frame writes are dropped or corrupt VRAM on real hardware; emulators vary — this is the classic "works in emulator, glitches on console" bug.
**Examples**: ✅ `vq_push(VRAM_BG1MAP+off, buf, 64)` in main loop. ❌ `REG_VMDATAL = x` anywhere outside NMI/force-blank.
**Enforcement**: single queue API; grep-gate in review (`prompts/code-review.md`); Mesen debugger break-on-invalid-write during test runs.

### INV-HW-002: Vblank upload ≤ 4KB/frame
**Rule**: Total queued DMA per frame ≤ 4096 bytes (NTSC vblank engineering margin).
**Rationale**: Overrun tears into active display, violating INV-HW-001 implicitly.
**Enforcement**: queue asserts in TEST builds (overflow sets a debug flag Lua tests check).

### INV-HW-003: The main loop waits for NMI
**Rule**: Exactly one sim step per displayed frame (`WAI`-synced); NMI handler completes within vblank.
**Rationale**: Torn frames and time-drift otherwise; determinism (INV-ENG-002) depends on it.
**Enforcement**: frame counter in debug block increments by exactly 1 per Lua-stepped frame.

### INV-HW-004: Nothing requires sprite rotation
**Rule**: No design or code may assume a sprite can rotate. Rotating visuals are Mode 7 BG or pre-drawn frames; chamber enemies/bosses are radially symmetric by design.
**Rationale**: SNES OBJ hardware flips only; it never rotates.
**Enforcement**: design review of every chamber entity; GDD lists compliant enemy shapes.

### INV-HW-005: OAM within per-line limits by construction
**Rule**: Worst-case layouts stay ≤32 sprites and ≤34 slivers per scanline (entity cap 16 + meta-sprite layouts audited); a flicker manager handles the residual worst case.
**Enforcement**: worst-case scripted scene in test suite; Mesen overlay check during phase gates.

### INV-HW-006: Chamber palette split
**Rule**: Mode 7 chamber tiles use CGRAM colors 0–127 only; sprites own 128–255.
**Rationale**: Mode 7 is 8bpp — tiles and OBJ share CGRAM; collisions rainbow the HUD.
**Enforcement**: asset converter flag rejects chamber tilesets with colors >127.

---

## Engine Invariants

### INV-ENG-001: Fixed-point only — floats are forbidden
**Rule**: Positions s32 16.8; velocities s16 8.8. No `float`/`double` anywhere in `src/`.
**Rationale**: 65816 has no FPU; tcc816 soft-float would crater the frame budget.
**Enforcement**: CI grep gate; review checklist.

### INV-ENG-002: Deterministic simulation
**Rule**: Same ROM + same input script ⇒ bit-identical debug-block state. RNG is seeded; no uninitialized RAM is read; no logic depends on uninitialized OAM/VRAM.
**Rationale**: Golden replay tests are the project's regression backbone.
**Enforcement**: `test_replay.lua` runs every suite; stays green forever.

### INV-ENG-003: The gravity/portal state space is closed
**Rule**: Gravity ∈ exactly 4 axis states; portal orientations ∈ 4; every (in, out) pair resolves through the single 16-entry transform LUT — no special-case teleport math anywhere else.
**Rationale**: Scattered orientation math is where subtle physics bugs breed.
**Enforcement**: LUT unit-covered by scripted tests for all 16 combos (Phase 2/3 gates).

### INV-ENG-004: Portal uniqueness & placement safety
**Rule**: ≤1 blue + ≤1 gold portal exist at any instant; placement re-validates surface material, clearance, and entity occupancy at placement time; teleport re-validates the pair before moving anything.
**Enforcement**: placement/teleport share one validator; `test_portal_rules.lua`.

### INV-ENG-005: Room data round-trips exactly
**Rule**: Tiled source → converter → ROM → engine-loaded metatile grid preserves collision + material for every cell (converter emits checksum; TEST build re-hashes on load).
**Rationale**: The classic cross-boundary seam — level bugs that look like physics bugs.
**Enforcement**: checksum compare surfaced in debug block; per-room load test.

### INV-ENG-006: Saves never brick
**Rule**: SRAM slots are versioned + checksummed; a corrupt/mismatched slot reads as "empty," never crashes, never half-loads.
**Enforcement**: `test_save_roundtrip.lua` includes a corrupted-SRAM case.

---

## Test & Audio Contracts

### INV-TEST-001: Debug block is an append-only contract
**Rule**: TEST-build block at `$7EFF00` (magic `0x51FE`; re-pinned from `$7E1F00`, D-010 — crt0 stack collision) only ever *appends* fields; existing offsets never move or change meaning. Includes the Lua→game warp-request mailbox.
**Rationale**: Every Lua test depends on these offsets.
**Enforcement**: layout table in `tests/README.md` is the source of truth; review gate.

### INV-AUD-001: The soundbank fits ARAM
**Rule**: Driver + samples + longest module ≤ 64KB together; 2 of 8 voices reserved for SFX.
**Enforcement**: build-time size assert on `smconv` output; worst-case-chaos audio test.

---

## Game Design Invariants

### INV-GAME-001: No permanent softlocks
**Rule**: From any reachable state (any portal placement, any crate position, any gravity), the current room remains exitable or resettable (room-reset on death re-randomizes nothing — see INV-ENG-002).
**Rationale**: Portal games generate unreachable-state bugs; a stuck player on a cartridge has no patch.
**Enforcement**: per-room design review checklist + scripted stuck-hunt playthroughs on each zone gate.

---

## Testing Requirements

Every invariant maps to at least one of: a scripted Lua test, a build-time assert, or a named review-checklist item. An invariant with no enforcement is a wish — fix it or delete it.

## Invariant Change Log

| Date | ID | Change | Reason | Approved |
|---|---|---|---|---|
| 2026-07-07 | all | Initial set (planning) | — | Jeremy (pending Phase 0 reality-check) |
| 2026-07-12 | INV-ENG-004 | Teleport revalidation refined to the EXIT portal only (placement still validates fully; one shared validator) | The entry plane was just crossed — terrain is immutable and an entity behind the mover can't endanger the transit; the redundant entry check cost measured transit-frame lag. Revisit when destructible/moving surfaces exist (magnet-plates, Phase 5). | Jeremy 2026-07-12 |

---
**Last Updated**: 2026-07-07
