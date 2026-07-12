/* tuning.h — every gameplay knob lives HERE and only here (CLAUDE.md).
 * Fixed point: positions s32 16.8, velocities s16 8.8 (INV-ENG-001).
 * Changing anything that moves the golden numbers (test_jump apex,
 * test_replay) requires a DECISIONS.md entry. */
#ifndef SKYFELL_GAME_TUNING_H
#define SKYFELL_GAME_TUNING_H

/* --- player run (GDD "Movement feel": run 1.5 px/f, apex ~2 metatiles,
 * gravity 0.1875, terminal 4 — the slow base makes portal flings at 6 px/f
 * read as dramatic, the momentum pillar) --- */
#define P_WALK_ACC 0x0028  /* 8.8 px/f^2: ground+air acceleration */
#define P_WALK_MAX 0x0180  /* 8.8: 1.5 px/f top run speed */
#define P_FRICTION 0x0020  /* 8.8: decel toward 0 with no input */

/* --- player jump/fall --- */
#define P_GRAV 0x0030     /* 8.8: 0.1875 px/f^2 */
#define P_JUMP_VY -0x03C0 /* 8.8: -3.75 px/f impulse (apex ~37px = 2.3 mt) */
#define P_TERM_VY 0x0400  /* 8.8: 4 px/f terminal fall */
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
/* airborne: vertical follow ONLY near the screen edges (the classic SNES
 * platformer rule — a camera that bobs with jump arcs feels bad AND streams
 * BG rows every jump; it cost 32 measured lag frames) */
#define CAM_AIR_Y0 40
#define CAM_AIR_Y1 190

/* --- spawn (room 0, px: box top-left) --- */
#define SPAWN_X 88
#define SPAWN_Y 418

/* --- the Rift Gun (Phase 2, GDD specs) --- */
#define SHOT_SPD 0x0400    /* 8.8: 4 px/f portal shot */
#define TP_COOLDOWN 8      /* frames before the same object re-enters */
#define TP_SPEED_CAP 0x0600 /* 6 px/f per axis after a teleport */
#define TP_EJECT_MIN 0x0100 /* 1 px/f guaranteed off the exit surface */

/* --- entities --- */
#define CRATE_W 14
#define CRATE_H 14
#define CRATE_PUSH 0x00C0  /* 0.75 px/f while the player pushes */
#define CRATE_FRIC 0x0018
#define THROW_VX 0x0280    /* crate throw impulse */
#define THROW_VY -0x0200
#define GRAB_RANGE 20      /* px from player center to a crate center */
#define SENTRY_PERIOD 96   /* frames between sentry shots */
#define SENTRY_RANGE 224   /* px: fires only with Wren inside ~a screen */
#define SSHOT_SPD 0x0200   /* 2 px/f sentry shot */
#define SHOT_BOX 6         /* shots use a 6x6 box */

/* --- the gravity chamber (Phase 3) --- */
#define ROT_STEP 2       /* theta units/frame: 64 units (90 deg) in 32f */
#define CHAM_SPAWN_X 512 /* world px, box top-left, on the arena floor */
#define CHAM_SPAWN_Y 706

/* --- camera --- */
#define CAM_EASE_X 16 /* max camera px/frame (teleport snap-ease) */
#define CAM_EASE_Y 8  /* vertical: one BG row stream per frame max — two
                         row-builds in one frame were measured lag frames */

#endif
