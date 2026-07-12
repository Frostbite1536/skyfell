/* entity.h — the 16-slot pool (ARCHITECTURE): crates, sentries, portal
 * shots, sentry shots. SoA parallel arrays (tcc816: never a struct array in
 * a hot path); dispatch is an if-else chain (the u8-switch trap). */
#ifndef SKYFELL_GAME_ENTITY_H
#define SKYFELL_GAME_ENTITY_H

#include <snes.h>

#define ENT_MAX 16

#define ET_NONE 0
#define ET_CRATE 1
#define ET_SENTRY 2
#define ET_SHOT_B 3 /* blue portal shot */
#define ET_SHOT_G 4 /* gold portal shot */
#define ET_SSHOT 5  /* sentry shot */

void ent_clear_all(void);
void ent_wake_all(void);             /* portal changes wake sleeping crates */
void ent_room_init(u8 room);         /* clear + this room's authored spawns */
u8 ent_spawn(u8 type, u16 x, u16 y); /* box top-left px; 0xFF if pool full */
void ent_set_face(u8 slot, u8 face); /* sentry aim: 0 right, 1 left */
void ent_set_vel(u8 slot, s16 vx, s16 vy);
void ent_update(void);               /* one frame for the whole pool */
void ent_render(void);               /* OAM sprites 2..17 */
u8 ent_count(void);

/* portal placement validator support (INV-ENG-004): does any live body
 * overlap this tile? */
u8 ent_occupies_tile(u16 tx, u16 ty);

/* player coupling (called from player.c):
 *  - clamp_x/clamp_y treat crates as walls for the mover's box; clamp_x
 *    also imparts push velocity to a grounded-pushed crate.
 *  - grab_toggle: A near a crate lifts it; A while carrying throws it. */
u8 ent_clamp_x(s32 *mpx, s16 *mvx, s16 y, u8 w, u8 h, u8 push);
u8 ent_clamp_y(s32 *mpy, s16 *mvy, s16 x, u8 w, u8 h); /* 1 = stood on crate */
void ent_grab_toggle(void);

#endif
