.include "hdr.asm"

; Data linked into ROM. Phase 0: only the placeholder text font (pvsneslib's
; example font, converted by gfx4snes into src/data/generated/ — gitignored,
; never hand-edit, never commit; the Makefile regenerates it). Skyfell's own
; art arrives via gfx4snes/tmx2snes from assets/ starting Phase 1.
;
; Pattern rules (prophet lessons): incbin labels are consumed from C as bare
; &label at the call site — never a stored C pointer (tcc816 derefs go
; data-bank-relative into WRAM). A wla-dx section must fit a single 32KB LoROM
; bank; when bundles grow, give each its own superfree section.

.section ".rodata1" superfree

; Phase 1 BG tileset (tools/art/mktiles.py — deterministic generated art)
tiles_chr:
.incbin "src/data/generated/tiles.chr"
tiles_chr_end:

tiles_pal:
.incbin "src/data/generated/tiles.pal"

.ends
