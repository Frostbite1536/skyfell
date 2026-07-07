# tests/ — emulator-level verification

Tests run the **real ROM** in MesenCE headless: `Mesen --testrunner build/skyfell-test.sfc tests/lua/<case>.lua` (flag confirmed in Mesen2 source `UI/Utilities/CommandLineHelper.cs`; exact MesenCE invocation gets proven in Phase 0 and recorded here). Each Lua script drives input frame-by-frame, reads game state from the debug block, asserts, screenshots, and exits via `emu.stop(code)` — exit code 0 = pass.

`tools/run_tests.py` discovers `tests/lua/test_*.lua`, runs each, collects screenshots into `artifacts/<test>/`, and prints the pass/fail table that gets recorded in `docs/CONTINUATION.md`.

## Debug block contract — `$7E1F00`, TEST builds only (INV-TEST-001: append-only)

| Offset | Type | Field |
|---|---|---|
| +0 | u16 | magic `0x51FE` |
| +2 | u16 | frame counter (+1 per frame, INV-HW-003) |
| +4 | s32 | player x (16.8) |
| +8 | s32 | player y (16.8) |
| +12 | s16 | player vx (8.8) |
| +14 | s16 | player vy (8.8) |
| +16 | u8 | gravity dir (0=down 1=left 2=up 3=right) |
| +17 | u8 | player FSM state |
| +18 | u8 | room id |
| +19 | u8 | portal blue: bit7 placed, bits0-1 orient |
| +20 | u8 | portal gold: same packing |
| +21 | u8 | entity count |
| +22 | u8 | vblank-queue overflow flag (INV-HW-002) |
| +23 | u8 | room checksum status (INV-ENG-005) |
| +24 | u16 | warp-request mailbox (Lua writes room id + 0x8000; game consumes) |
| +26… | — | **append new fields here; never repack** |

## Planned harness helpers (`tests/lua/lib/harness.lua`)

`waitFrames(n)` · `press(buttons, frames)` · `readU8/readU16/readS32(off)` · `assertEq(got, want, msg)` · `snap(name)` · `warp(room)` · `pass()/fail(msg)`

## Test inventory (grows per phase — see ROADMAP gates)

Phase 0: `test_boot` · Phase 1: `test_walk`, `test_jump` (golden apex), `test_replay` (determinism, runs forever) · Phase 2: `test_fling`, `test_portal_rules`, `test_sentry` · Phase 3: `test_gravity_cycle`, `test_chamber_puzzle` · Phase 4: `test_save_roundtrip`, `test_gate`

Golden values live beside their tests; changing one requires a DECISIONS.md entry.
