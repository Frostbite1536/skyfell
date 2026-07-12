/* tuning.h — every gameplay knob lives HERE and only here (CLAUDE.md).
 * Fixed point: positions s32 16.8, velocities s16 8.8 (INV-ENG-001).
 * Changing anything that moves the golden numbers (test_jump apex,
 * test_replay) requires a DECISIONS.md entry. */
#ifndef SKYFELL_GAME_TUNING_H
#define SKYFELL_GAME_TUNING_H

/* --- player run --- */
#define P_WALK_ACC 0x0030  /* 8.8 px/f^2: ground+air acceleration */
#define P_WALK_MAX 0x0280  /* 8.8: 2.5 px/f top run speed */
#define P_FRICTION 0x0028  /* 8.8: decel toward 0 with no input */

/* --- player jump/fall --- */
#define P_GRAV 0x0038     /* 8.8: 0.22 px/f^2 */
#define P_JUMP_VY -0x04C0 /* 8.8: -4.75 px/f impulse (apex ~51px) */
#define P_TERM_VY 0x0500  /* 8.8: 5 px/f terminal fall */
#define P_COYOTE_F 4      /* frames of jump grace after leaving a ledge */
#define P_JBUF_F 4        /* frames a jump press waits for the ground */

/* --- player collision box (px), inside the 16x32 sprite --- */
#define PB_W 10
#define PB_H 30
#define PB_SPR_DX 3 /* sprite left = box left - this */
#define PB_SPR_DY 2 /* sprite top  = box top  - this */

/* --- camera deadzone (screen px) --- */
#define CAM_DEAD_X0 120
#define CAM_DEAD_X1 136
#define CAM_DEAD_Y0 100
#define CAM_DEAD_Y1 124

/* --- spawn (room 0, px: box top-left) --- */
#define SPAWN_X 88
#define SPAWN_Y 418

#endif
