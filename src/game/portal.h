/* portal.h — the Rift Gun's two linked portals (Phase 2).
 * Orientation = the OUTWARD normal of the surface the portal sits on:
 * 0 up (floor), 1 right, 2 down (ceiling), 3 left — the closed state space
 * of INV-ENG-003. All teleport math goes through the single 16-entry LUT
 * in portal.c; placement and teleport share ONE validator (INV-ENG-004). */
#ifndef SKYFELL_GAME_PORTAL_H
#define SKYFELL_GAME_PORTAL_H

#include <snes.h>

#define PC_BLUE 0
#define PC_GOLD 1

void portal_init(void);  /* statics are WRAM garbage: call at boot/room load */
void portal_clear(void); /* recall both (Select) */

/* Place `color` on the surface hit at tile (tx,ty) with outward normal
 * `orient`. Snaps a 6-tile strip centered on the hit, sliding +-2 tiles to
 * fit. Validates: every strip tile solid+brass, every front tile empty, no
 * other-portal overlap, no entity occupancy. Returns 1 placed / 0 fizzled
 * (dbg_fizz counts). Refiring a color moves it. */
u8 portal_try_place(u8 color, u16 tx, u16 ty, u8 orient);

/* room.c overlay hooks. portal_any is the inline hot-path gate: callers on
 * per-tile paths MUST check it before calling (a byte load vs a tcc816
 * call — the difference was measured lag frames). */
extern u8 portal_any;                /* 1 while any portal is placed */
u16 portal_map_word(u16 tx, u16 ty); /* BG word for a portal cell, 0 = none */
u8 portal_cell(u16 tx, u16 ty);      /* 1 if a portal covers this tile */
/* patch portal cells into a slot-ordered seam buffer (room.c streaming):
 * fix_row: buf[64], slots wx&63, window cols wx0..wx0+63, world row wy;
 * fix_col: buf[32], slots wy&31, window rows wy0..wy0+31, world col wx. */
void portal_fix_row(u16 wy, u16 *buf, u16 wx0);
void portal_fix_col(u16 wx, u16 *buf, u16 wy0);

/* Teleport check for any mover (player or entity): box top-left 16.8,
 * box w/h in px, velocity 8.8, per-object cooldown counter. On a plane
 * crossing (center inside the opening, velocity into the surface, both
 * portals valid) rewrites pos+vel through the LUT, caps speed at 6 px/f,
 * guarantees 1 px/f ejection, sets the 8-frame cooldown. Returns 1. */
u8 portal_check(s32 *mx, s32 *my, s16 *mvx, s16 *mvy, u8 *cool, u8 w, u8 h);

u8 portal_on(u8 color);

#endif
