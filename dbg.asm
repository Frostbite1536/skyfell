.include "hdr.asm"

; SKYFELL debug/test block — INV-TEST-001: append-only; offsets are the
; contract in tests/README.md. Compiled into every build; only TEST builds
; write it (src/main.c guards with #ifdef TEST_BUILD).
;
; Placement: $7E:FF00 (D-010) — top page of WRAM bank $7E, which is the
; run-time data bank (crt0_snes.asm sets DBR=$7E), so C code reaches these
; labels with plain absolute addressing and Lua reads 0x7EFF00.
; The planning-time address $7E1F00 was REJECTED (D-010, proven in sibling
; prophet's Phase 0): crt0_snes.asm (4.5.0, line 244) sets the stack to $1FFF
; growing DOWN through the $1F00 page. FORCE makes wlalink fail loudly on any
; section collision.

; BANK 126 = $7E: tcc addresses these labels with absolute-long (sta.l), so the
; section's bank byte must be the real WRAM bank — BANK 0 sends writes into ROM
; space at $00:FF00 (prophet found that the hard way).
.RAMSECTION ".dbg" BANK 126 SLOT 2 ORGA $FF00 FORCE
dbg_magic     dsw 1  ; +0  u16  0x51FE while a TEST build is alive
dbg_frame     dsw 1  ; +2  u16  +1 per main-loop frame (INV-HW-003)
dbg_px        dsw 2  ; +4  s32  player x, 16.8 (Phase 1)
dbg_py        dsw 2  ; +8  s32  player y, 16.8 (Phase 1)
dbg_vx        dsw 1  ; +12 s16  player vx, 8.8 (Phase 1)
dbg_vy        dsw 1  ; +14 s16  player vy, 8.8 (Phase 1)
dbg_grav      dsb 1  ; +16 u8   gravity dir (0=down 1=left 2=up 3=right)
dbg_fsm       dsb 1  ; +17 u8   player FSM state (Phase 1)
dbg_room      dsb 1  ; +18 u8   room id (Phase 1)
dbg_pblue     dsb 1  ; +19 u8   portal blue: bit7 placed, bits0-1 orient (Phase 2)
dbg_pgold     dsb 1  ; +20 u8   portal gold: same packing (Phase 2)
dbg_entn      dsb 1  ; +21 u8   entity count (Phase 2)
dbg_vqovf     dsb 1  ; +22 u8   vblank-queue overflow flag (INV-HW-002)
dbg_roomck    dsb 1  ; +23 u8   room checksum status (INV-ENG-005)
dbg_warp      dsw 1  ; +24 u16  warp-request mailbox: Lua writes room id +
                     ;          0x8000; game consumes by writing 0 (Phase 1)
; --- Phase 1 appends (2026-07-11) ---
dbg_vbl_last  dsw 1  ; +26 u16  bytes the drain moved last NMI
dbg_vbl_max   dsw 1  ; +28 u16  high-water of dbg_vbl_last
dbg_flags     dsw 1  ; +30 u16  bit0 = vblank-queue overflow (dbg_vqovf mirrors)
dbg_vbl_defer dsw 1  ; +32 u16  cumulative entries deferred to a later NMI
dbg_lag       dsw 1  ; +34 u16  lag_frame_counter mirror (60fps gate)
dbg_cmd       dsw 1  ; +36 u16  Lua test mailbox verb (tests/README.md)
dbg_arg0      dsw 1  ; +38 u16  mailbox arg0
dbg_arg1      dsw 1  ; +40 u16  mailbox arg1
dbg_vbl_v     dsw 1  ; +42 u16  scanline where the drain last started work
; --- append new fields at +44; never repack (INV-TEST-001) ---
.ENDS
