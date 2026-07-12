/* Lua->game debug mailbox (INV-TEST-001 block, offsets +36..+40). TEST
 * builds poll once per frame; release builds compile this to a no-op.
 * Verbs (the contract lives in tests/README.md):
 *   1 RESEED    arg0=seed lo16, arg1=seed hi16 (0 = default seed)
 *   2 VQ_STRESS arg0=n entries, arg1=base pattern — n live 2-word VRAM
 *               pushes at test-scratch word addr 0x7C00+2i
 *   3 VQ_BUDGET arg0=bytes (0 = back to the measured budget)
 * The game acks by writing 0 back to dbg_cmd.
 * (The room-warp request is its own mailbox at +24 — room.c consumes it.) */
#ifndef SKYFELL_CORE_DBGCMD_H
#define SKYFELL_CORE_DBGCMD_H

#include <snes.h>

void dbg_poll(void);

#endif
