.include "hdr.asm"

; LIVE world maps in WRAM bank $7F — copies of the generated ROM data made
; under force-blank at load. WRAM because worlds MUTATE (the chamber's
; puzzle door opens; magnet plates later) and because ONE label keeps
; every solid reader and the seam builders direct-indexed with zero
; hot-path indirection (the measured-lag rule):
;   cham_map — chamber_load copies cham_rom_map (chamber.asm)
;   room_map — room_load copies the selected roomNN_map (rooms.asm);
;              seams.asm MVN/gathers read it, so ALL rooms stream through
;              this one label (D-016)
; ORGA $0100: pvsneslib claims $7F:0000-$003F. FORCE makes wlalink fail
; loudly on any collision.
.RAMSECTION ".chamram" BANK 127 SLOT 3 ORGA $0100 FORCE
cham_map dsb 16384 ; 128x128 tile bytes, mirrors VRAM $0000-$3FFF low bytes
room_map dsw 8192  ; 128x64 baked map WORDS (D-012 grid)
.ENDS
