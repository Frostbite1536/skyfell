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
#define ET_DRONE 6  /* Gale Drone: leashed chamber floater (D-014) */

void ent_clear_all(void);
void ent_wake_all(void);             /* portal/gravity changes wake crates */
extern u8 ent_slept; /* crate-sleep event flag (chamber.c pad probe gate) */
extern u8 ent_hit_player; /* sentry shot connected (main.c death, D-018) */
void ent_room_init(u8 room);         /* clear + this room's authored spawns */
u8 ent_spawn(u8 type, u16 x, u16 y); /* box top-left px; 0xFF if pool full */
void ent_set_face(u8 slot, u8 face); /* sentry aim: 0 right, 1 left */
void ent_set_vel(u8 slot, s16 vx, s16 vy);
void ent_update(void);               /* one frame for the whole pool */
void ent_render(void);               /* OAM sprites 2..17 */
u8 ent_count(void);

/* portal placement validator support (INV-ENG-004): does any live body
 * except `exclude` (0xFF = none) overlap the INCLUSIVE tile rect? One pass
 * over the live list — the per-tile variant cost measured lag frames. */
u8 ent_occupies_rect(u16 tx0, u16 ty0, u16 tx1, u16 ty1, u8 exclude);

/* puzzle probe: any crate asleep with box top == ytop, center x in
 * [x0,x1]? (chamber.c's ceiling pressure pad, D-015) */
u8 ent_crate_resting(u16 x0, u16 x1, u16 ytop);

/* player coupling (called from player.c):
 *  - clamp_x/clamp_y treat crates as walls for the mover's box; clamp_x
 *    also imparts push velocity to a grounded-pushed crate.
 *  - grab_toggle: A near a crate lifts it; A while carrying throws it. */
u8 ent_clamp_x(s32 *mpx, s16 *mvx, s16 y, u8 w, u8 h, u8 push);
u8 ent_clamp_y(s32 *mpy, s16 *mvy, s16 x, u8 w, u8 h); /* 1 = stood on crate */
void ent_grab_toggle(void);

#endif
