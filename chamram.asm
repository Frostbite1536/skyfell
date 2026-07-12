.include "hdr.asm"

; The chamber's LIVE map: a 16KB WRAM copy (bank $7F) of the generated ROM
; map (chamber.asm: cham_rom_map), made under force-blank in chamber_load.
; WRAM because the world MUTATES (the puzzle's exit door opens; magnet
; plates later) and every solid reader — chamber.c csolid/craw_box,
; entity.c solid_at, portal.c's chamber validator — must see ONE truth
; with zero hot-path indirection (direct lda.l label indexing, the
; measured-lag rule). ORGA $0100: pvsneslib claims $7F:0000-$003F.
; FORCE makes wlalink fail loudly on any collision.
.RAMSECTION ".chamram" BANK 127 SLOT 3 ORGA $0100 FORCE
cham_map dsb 16384 ; 128x128 tile bytes, mirrors VRAM $0000-$3FFF low bytes
.ENDS
