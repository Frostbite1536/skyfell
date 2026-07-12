/* vblank.h — Phase 0 STUB of the vblank queue (INV-HW-001: the queue is the
 * only sanctioned PPU-write path outside force-blank). Phase 1 replaces this
 * with the full engine (SoA entries, precomputed DMA, measured per-NMI byte
 * budget with tail-defer — port from sibling prophet's src/core/vblank.c);
 * the API seam below is what survives.
 *
 * Stub scope: queued CGRAM single-color writes, drained inside the lib's
 * VBlank ISR via nmiSet(vq_nmi). Enough for the Phase 0 backdrop color cycle
 * to be INV-HW-001-clean. */

#ifndef SKYFELL_CORE_VBLANK_H
#define SKYFELL_CORE_VBLANK_H

#include <snes.h>

void vq_init(void);                        /* WRAM is boot garbage: call before nmiSet */
void vq_push_cgram(u8 entry, u16 bgr555);  /* queue one CGRAM color write */
void vq_nmi(void);                         /* NMI-time drain — install with nmiSet() */

extern u8 vq_overflow; /* sticky; TEST builds mirror it to dbg block +22 (INV-HW-002) */

#endif
