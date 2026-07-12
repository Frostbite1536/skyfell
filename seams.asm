.include "hdr.asm"

; Seam builders (src/game/room.c) — the profiled hot path dropped to asm per
; D-001: tcc816's C build-then-copy cost ~90 scanlines per streamed column;
; these fill the vblank queue's staging area (vqdata.asm, $7E:F000) straight
; from the room map in ROM.
;
; Params are C globals in room.c (bank $7E = the runtime data bank, so
; plain absolute addressing reaches them):
;   sm_src  u16  BYTE offset into room01_map
;   sm_dst  u16  in-bank ($7E) BYTE address of the destination
;   sm_cnt  u16  seam_mvn: BYTE count / seam_coln: WORD count
; Both are void(void), jsl-called from C, clobber A/X/Y (tcc816 caller-saved).

.section ".seams" superfree

; contiguous run: MVN block move at 7 cycles/byte (a full 128-byte row run
; lands in ~900 cycles vs ~20k for the C loop)
seam_mvn:
    php
    rep #$30            ; 16-bit A/X/Y
    lda.w sm_src
    clc
    adc #room01_map     ; in-bank source address
    tax
    ldy.w sm_dst
    lda.w sm_cnt
    dec a               ; MVN takes count-1
    .db $54, $7E        ; MVN: opcode, DEST bank ($7E = vq_data/WRAM)...
    .db :room01_map     ; ...SOURCE bank (the room data's ROM bank)
    ; MVN leaves DBR = $7E, which IS the runtime data bank — no restore
    plp
    rtl

; strided column run: one word every 128 map words (256 bytes), sm_cnt words
seam_coln:
    php
    rep #$30
    ldx.w sm_src
    ldy.w sm_dst
-   lda.l room01_map,x
    sta.w $0000,y
    iny
    iny
    txa
    clc
    adc #256            ; next map row, same column
    tax
    dec.w sm_cnt
    bne -
    plp
    rtl

.ends
