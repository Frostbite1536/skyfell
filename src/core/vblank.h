/* Vblank queue — the ONLY path to PPU memory during active display
 * (INV-HW-001). Game code pushes writes during the frame; the NMI handler
 * drains them inside vblank by DMA, then applies the scroll shadows.
 *
 * Ported from sibling prophet's hardened drain (its D-061): SoA entries with
 * push-time-precomputed DMA registers, a measured per-NMI byte budget from
 * the latched vertical counter, and tail-DEFER to the next NMI instead of
 * kicking DMAs into active display. Budget constants re-derived on skyfell's
 * own ROM (vblank.c).
 *
 * Concurrency contract (pvsneslib VBlank ISR, interrupt.h): the drain only
 * runs when vblank_flag is set, i.e. when the main loop is parked in
 * WaitForVBlank() — main never races the drain. On a lag frame the drain is
 * skipped and the queue keeps accumulating (dbg_lag mirrors the lib's
 * lag_frame_counter in TEST builds). */
#ifndef SKYFELL_CORE_VBLANK_H
#define SKYFELL_CORE_VBLANK_H

#include <snes.h>

void vq_init(void);    /* empty the queue (also after force-blank redraws) */
void vq_install(void); /* vq_init + nmiSet(drain); call once at boot        */

/* Push copies src, so callers may reuse their buffer immediately.
 * Returns 0 and sets dbg_flags bit0 (TEST) if the frame budget is full. */
u8 vq_push_vram_seq(u16 vaddr, const u16 *src, u16 words); /* +1 word stride */
u8 vq_push_vram_col(u16 vaddr, const u16 *src, u8 words);  /* +32 word stride:
                                       one tilemap column inside a screen    */
u8 vq_push_cgram(u8 color, const u16 *src, u8 count);      /* CGRAM colors   */

/* Scroll shadows — applied atomically at the top of every drained NMI, so a
 * frame's streamed columns always pair with their scroll. */
void vq_set_scroll(u16 bg1x, u16 bg1y);
void vq_set_scroll_bg2(u16 bg2x, u16 bg2y);

#ifdef TEST_BUILD
void vq_test_budget(u16 bytes); /* override the drain's byte budget (0 = off) */
#endif

/* Force-blank helpers — direct writes, legal ONLY under REG_INIDISP=0x80
 * (boot / room transitions). Not queue clients. */
void fb_vram_write(u16 vaddr, const u16 *src, u16 words);
void fb_vram_fill(u16 vaddr, u16 value, u16 words);

#endif
