# tests/ — emulator-level verification

Tests run the **real ROM** in MesenCE headless. The invocation proven in Phase 0 (2026-07-11), per test:
`Mesen.exe --testrunner --enablestdout --timeout=120 build/skyfell-test.sfc <combined.lua>` — switches NEED the `--` prefix (bare `timeout=60` is treated as a missing file), `DOTNET_ROOT` must point at the user-local .NET (run_tests.py injects it), and the gate signal is the process **exit code** from `emu.stop(code)` (0 = pass; emu.log output is invisible in testrunner mode). Each Lua script drives input frame-by-frame, reads game state from the debug block, asserts, screenshots, and stops.

`tools/run_tests.py` discovers `tests/lua/test_*.lua`, runs each, collects screenshots into `artifacts/<test>/`, and prints the pass/fail table that gets recorded in `docs/CONTINUATION.md`.

## Debug block contract — `$7EFF00`, TEST builds only (INV-TEST-001: append-only)

*(Re-pinned from the planning-time `$7E1F00` per D-010: pvsneslib 4.5.0's crt0 puts the stack at `$1FFF` growing down through that page. Declared `BANK 126 SLOT 2 ORGA $FF00 FORCE` in `dbg.asm` — bank 126 = `$7E`, required because tcc816 addresses the labels with absolute-long `sta.l`.)*

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

## Harness helpers (`tests/lua/lib/harness.lua`, ported from prophet)

`H.waitAlive()` (call first, always) · `H.waitFrames(n)` · `H.waitUntil(pred, max, what, code)` · `H.maskInput()` (call before `H.run`; keyboard can never leak into a deterministic run) · `H.padScript(fn)` (scripted pad as a function of `dbg_frame`) · `H.readU8/readU16`, `H.dbgU8/dbgU16(off)`, `H.vramWord/cgramWord` · `H.assertEq(got, want, what, code)` · `H.snap(name)` · `H.warp(room)` (game side lands Phase 1) · `H.pass()/H.fail(msg, code)` · `H.run(body)`

Stop-code convention: 0 pass · 1 generic · 12 Lua error · 13 body-ended-without-pass · tests use 10+ (skipping 12/13). The runner retries ONCE on rc<0 only (emulator death, never an assert).

## Test inventory (grows per phase — see ROADMAP gates)

Phase 0: `test_boot` · Phase 1: `test_walk`, `test_jump` (golden apex), `test_replay` (determinism, runs forever) · Phase 2: `test_fling`, `test_portal_rules`, `test_sentry` · Phase 3: `test_gravity_cycle`, `test_chamber_puzzle` · Phase 4: `test_save_roundtrip`, `test_gate`

Golden values live beside their tests; changing one requires a DECISIONS.md entry.
