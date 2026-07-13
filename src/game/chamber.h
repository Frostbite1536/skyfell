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

/* chamber sprite transform (entity.c ent_render) — plain globals, not
 * accessors: a tcc816 call is ~1k cycles. While cham_rot==1 the WORLD is
 * frozen: main.c skips ent_update and ent_render hides every entity sprite
 * (they cannot rotate with the screen, INV-HW-004). At rest, world->screen
 * for a sprite = 90-degree swap (by cham_thk) of its offset from Wren's
 * box center (cham_cx/cy = the M7 pivot, pinned at screen 128,112). */
extern u8 cham_rot;  /* 1 while the rotation tween runs (physics frozen) */
extern u8 cham_thk;  /* theta>>6: 0..3 quarter-turns (valid at rest) */
extern s16 cham_cx;  /* Wren's box center, world px */
extern s16 cham_cy;
extern s8 cham_gx;   /* gravity unit vector, world coords — chamber crate
                        physics + carried/drop directions (entity.c) */
extern s8 cham_gy;
extern u8 cham_exit; /* the puzzle exit recess was entered — main.c
                        consumes it (fade back to the hall, D-017) */

#endif
