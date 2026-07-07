# Bug hunt — Skyfell

Hunt for real, reachable defects in `src/` — report file:line, the failure scenario (inputs/state → wrong behavior), and severity per REVIEW.md. Verify each candidate against the actual code path before reporting; no style notes.

Priority hunting grounds (where SNES + LLM-generated code actually fails):

1. **Vblank discipline**: any PPU register/VRAM/OAM write reachable outside the NMI drain or force-blank (INV-HW-001); queued frames that can exceed 4KB (INV-HW-002)
2. **16-bit `int` traps**: intermediate overflow in physics/camera math (pos×vel, deltas near room edges); implicit promotions; signed/unsigned comparisons
3. **Bank/pointer bugs**: data tables crossing 32KB bank boundaries; far pointers passed as near
4. **Determinism leaks** (INV-ENG-002): uninitialized WRAM reads, unseeded RNG paths, order-dependent entity updates, anything reading OAM/VRAM back as game state
5. **Orientation math outside the LUT** (INV-ENG-003): grep for ad-hoc vx/vy swaps or negations in portal/gravity paths
6. **Boundary off-by-ones**: 16px metatile edges, portal-plane crossing detection at high speed (tunneling past 48px openings at 6px/f?)
7. **Round-trip seams** (INV-ENG-005/006): converter fields the engine never reads, save fields that don't restore, debug-block offsets that drifted from tests/README.md
8. **OAM pressure**: worst-case scenes exceeding 32 sprites/line (INV-HW-005)

For every confirmed bug: write the failing Lua test first, then propose the fix, then search the codebase for the same pattern class.
