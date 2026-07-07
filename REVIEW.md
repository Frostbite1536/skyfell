# REVIEW.md — code review rules for Skyfell

Severity levels (consistent across all reviews):
- **Critical** — blocks merge: invariant violation, correctness bug, determinism break
- **Nit** — fix if easy, never blocks
- **Pre-existing** — file separately, don't scope-creep the diff

Review changed files **in full context**, not just the diff. Then check, in order:

## 1. Invariant sweep (Critical by definition)
Walk `docs/INVARIANTS.md` top to bottom against the change. The usual suspects:
- PPU/VRAM/CGRAM/OAM touched outside the vblank queue or force-blank (INV-HW-001)
- New per-frame uploads pushing past the 4KB budget (INV-HW-002)
- Any float, or math that silently promotes to one (INV-ENG-001)
- Nondeterminism: uninitialized reads, unseeded RNG, frame-order dependence (INV-ENG-002)
- Orientation math done anywhere but the 16-entry LUT (INV-ENG-003)
- Debug-block offsets moved instead of appended (INV-TEST-001)

## 2. 65816/tcc816 traps (LLM-generated code fails here characteristically)
- `int` is 16-bit: overflow in intermediate arithmetic (esp. pos*vel, camera math)
- Bank crossing: data tables >32KB or pointers handed across banks without far handling
- DMA sizes/addresses in bytes vs. VRAM words confusion (×2 errors)
- Signed/unsigned comparison at metatile boundaries; off-by-one at 16px edges
- Hot-loop struct copies or multiplies that belong in a LUT

## 3. Cross-boundary integrity
- New field in an asset format? Verify converter → ROM data → engine reader → (if saved) SRAM all round-trip it (INV-ENG-005/006). Grep every producer and consumer.
- New debug-block field? Appended, documented in tests/README.md, harness updated.

## 4. Tests
- The change's behavior is exercised by a Lua test or golden replay — "compiles" is not evidence
- Golden values changed? Requires a DECISIONS.md entry, else Critical

Reviews are honest: paste `make test` output; a green suite you didn't run doesn't count.
