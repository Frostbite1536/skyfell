.include "hdr.asm"

; Vblank-queue payload staging (src/core/vblank.c). Lives at a FIXED address
; because the NMI drain sends it to the PPU by DMA, and the DMA source
; registers ($4362-64) want a compile-time bank:address — same trick as the
; debug block (dbg.asm). $7E:F000-$F3FF sits safely below the dbg block at
; $7E:FF00. BANK 126 = $7E (tcc reaches the label with absolute-long
; addressing); FORCE makes wlalink fail loudly on any collision.
.RAMSECTION ".vqdata" BANK 126 SLOT 2 ORGA $F000 FORCE
vq_data  dsw 512  ; payload staging, 1KB; offsets handed to the DMA engine
.ENDS
