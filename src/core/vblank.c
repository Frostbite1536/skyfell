/* vblank.c — Phase 0 vblank-queue stub (see vblank.h; INV-HW-001). */

#include "src/core/vblank.h"

#define VQ_CAP 8

/* Parallel plain arrays, not array-of-struct: tcc816 indexes a struct array
 * via a software-multiply helper (prophet's Phase 2 frame-budget lesson) —
 * the full Phase 1 queue keeps this shape, so the stub starts with it. */
static u16 vq_entry[VQ_CAP];
static u16 vq_color[VQ_CAP];
static u8 vq_n;
u8 vq_overflow;

void vq_init(void)
{
    vq_n = 0;
    vq_overflow = 0;
}

void vq_push_cgram(u8 entry, u16 bgr555)
{
    if (vq_n >= VQ_CAP)
    {
        vq_overflow = 1; /* sticky by design: a drop is a defect, not a hiccup */
        return;
    }
    vq_entry[vq_n] = entry;
    vq_color[vq_n] = bgr555;
    vq_n++;
}

/* The lib's DEFAULT nmi_handler (consoles.asm: consoleInit does
 * nmiSet(consoleVblank)) — it DMAs the console text buffer to the configured
 * map address when scr_txt_dirty is set. nmiSet(vq_nmi) displaces it, so we
 * chain it (interrupt.h:227). Not declared in any pvsneslib header.
 * (Do NOT use consoleUpdate() instead: it DMAs to a HARDCODED VRAM $0800,
 * ignoring consoleSetTextMapPtr — found the hard way in Phase 0.) */
extern void consoleVblank(void);

/* Runs inside the pvsneslib VBlank ISR (nmiSet), so these PPU writes land in
 * vblank by construction. The stub's whole payload is a few CGRAM colors —
 * no byte budget needed yet; the Phase 1 port brings the measured one. */
void vq_nmi(void)
{
    u8 i;
    u16 c;
    consoleVblank(); /* keep the lib console alive under our handler */
    for (i = 0; i < vq_n; i++)
    {
        REG_CGADD = (u8)vq_entry[i];
        c = vq_color[i];
        *CGRAM_PALETTE = (u8)c;        /* $2122: low byte first */
        *CGRAM_PALETTE = (u8)(c >> 8); /* then high (BGR555 word) */
    }
    vq_n = 0;
}
