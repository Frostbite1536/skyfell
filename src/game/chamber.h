/* chamber.h — the Mode 7 gravity chamber (Phase 3, D-004/D-005).
 * World data never rotates: physics runs in world coords with a 4-state
 * gravity vector; the SCREEN rotates (M7 matrix) so gravity reads as down;
 * Wren draws upright, pinned at screen center (sprites cannot rotate,
 * INV-HW-004). */
#ifndef SKYFELL_GAME_CHAMBER_H
#define SKYFELL_GAME_CHAMBER_H

#include <snes.h>

void chamber_load(void);        /* force-blank: Mode 7 world + OBJ rebase */
void chamber_frame(u16 pad);    /* physics (gravity frame) + rotation SM */
void chamber_render(void);      /* pinned player sprite */
void chamber_warp(u16 x, u16 y); /* test verb 4 in chamber mode */
void chamber_set_gravity(u8 g); /* 0 down,1 left,2 up,3 right + rotation */

#endif
