/* player.h — Wren: FSM + fixed-point physics + swept AABB collision vs the
 * room's tile attributes, camera deadzone-follow, and the 16x32 two-sprite
 * OAM render. All knobs in tuning.h. */
#ifndef SKYFELL_GAME_PLAYER_H
#define SKYFELL_GAME_PLAYER_H

#include <snes.h>

/* FSM states (dbg_fsm mirrors these) */
#define PF_IDLE 0
#define PF_RUN 1
#define PF_JUMP 2
#define PF_FALL 3

void player_obj_init(void);        /* force-blank: OBJ chr/pal + OAM hide-all */
void player_obj_base(u16 vram_words); /* rebase OBJ chr (rooms $2000; the
                                         Mode 7 chamber $4000) + hide-all */
void player_init(u16 x, u16 y);    /* place at box top-left (px), zero motion */
void player_warp(u16 x, u16 y);    /* player_init + force-blank camera warp */
void player_update(u16 pad);       /* one frame: input, physics, collision, camera */
void player_render(void);          /* OAM shadow writes (lib ISR DMAs it) */

/* entity coupling (entity.c reads these for carry/throw/grab) */
s16 player_px(void); /* box top-left, whole px */
s16 player_py(void);
u8 player_face(void); /* 0 right, 1 left */

#endif
