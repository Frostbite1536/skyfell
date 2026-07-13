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
u8 vq_push_m7map(u16 tile_addr, const u8 *src, u8 bytes); /* Mode 7 map
                                       bytes, +1 tile per byte             */

/* Zero-copy staging (the seam streamers' path — profiling showed the C
 * build-then-copy cost ~90 scanlines per column): reserve `words` of
 * payload for `entries` future commits, fill vq_data[off..] directly (the
 * asm seam builders write it), then commit each DMA run over the staged
 * range. Returns 0xFFFF (+ the overflow flag) when the frame is full. */
extern u16 vq_data[]; /* vqdata.asm, $7E:F000 */
u16 vq_stage(u16 words, u8 entries);
void vq_commit_seq(u16 vaddr, u16 off, u16 words);
void vq_commit_col(u16 vaddr, u16 off, u16 words);

/* Scroll shadows — applied atomically at the top of every drained NMI, so a
 * frame's streamed columns always pair with their scroll. */
void vq_set_scroll(u16 bg1x, u16 bg1y);
void vq_set_scroll_bg2(u16 bg2x, u16 bg2y);

/* Mode 7 shadows (the chamber): matrix A-D (8.8), pivot X/Y + scroll HOFS/
 * VOFS (13-bit), all latched and written together in the NMI — 16 register
 * stores, atomic per frame. vq_set_m7_on(0) back in Mode 1 rooms. */
void vq_set_m7_on(u8 on);
void vq_set_m7(s16 a, s16 b, s16 c, s16 d, s16 x, s16 y, s16 hofs, s16 vofs);

/* Brightness shadow (door fades, D-017): $2100 = b (0-15) applied inside
 * the NEXT NMI, one-shot ($2100 mid-display tears a scanline; the lib's
 * setScreenOn/Off still own load boundaries). */
void vq_set_bright(u8 b);

/* Console-text screens (title/end card, D-019): while 1, the NMI chains
 * the lib's consoleVblank text uploader. main.c owns the state. */
extern u8 vq_console;

#ifdef TEST_BUILD
void vq_test_budget(u16 bytes); /* override the drain's byte budget (0 = off) */
u16 vq_scanline(void); /* latch + read the current vertical counter (the
                          main loop's frame-cost probe, dbg_mainv +62) */
#endif

/* Force-blank helpers — direct writes, legal ONLY under REG_INIDISP=0x80
 * (boot / room transitions). Not queue clients. */
void fb_vram_write(u16 vaddr, const u16 *src, u16 words);
void fb_vram_fill(u16 vaddr, u16 value, u16 words);

#endif
