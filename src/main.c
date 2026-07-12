/* RIFT: THE SKYFELL ENGINE — Phase 1 (in progress).
 *
 * Unit B state: the metatile room is live — tileset + room01 through the
 * tmx2snes pipeline (D-011/D-012), camera with edge clamp and BG1 seam
 * streaming through the vblank queue (INV-HW-001). Until the player lands
 * (Unit C), the camera free-flies on the d-pad so `make run` can inspect
 * the room; tests drive it through the POS_SET mailbox verb. */

#include <snes.h>

#include "src/core/dbgcmd.h"
#include "src/core/rng.h"
#include "src/core/vblank.h"
#include "src/game/room.h"

#ifdef TEST_BUILD
/* Debug block at $7E:FF00 — labels in dbg.asm (INV-TEST-001, D-010). WRAM
 * boots as garbage: every field is zeroed below BEFORE dbg_magic goes live,
 * or the Lua harness could read phantom state (prophet's boot lesson). */
extern u16 dbg_magic;  /* +0:  0x51FE when alive        */
extern u16 dbg_frame;  /* +2:  +1 per main-loop frame   */
extern s32 dbg_px;     /* +4..+15: player kinematics (Unit C owns) */
extern s32 dbg_py;
extern s16 dbg_vx;
extern s16 dbg_vy;
extern u8 dbg_grav;    /* +16..+21: world state */
extern u8 dbg_fsm;
extern u8 dbg_room;    /* room.c owns live */
extern u8 dbg_pblue;
extern u8 dbg_pgold;
extern u8 dbg_entn;
extern u8 dbg_vqovf;   /* +22: vblank-queue overflow mirror (INV-HW-002) */
extern u8 dbg_roomck;  /* +23: room.c owns live */
extern u16 dbg_warp;   /* +24: Lua warp mailbox — consumed below */
extern u16 dbg_vbl_last;  /* +26..+32: vblank.c owns these live */
extern u16 dbg_vbl_max;
extern u16 dbg_flags;
extern u16 dbg_vbl_defer;
extern u16 dbg_lag;    /* +34: lag_frame_counter mirror */
extern u16 dbg_cmd;    /* +36..+40: Lua test mailbox (dbgcmd.c) */
extern u16 dbg_arg0;
extern u16 dbg_arg1;
extern u16 dbg_vbl_v;  /* +42: drain-start scanline (vblank.c) */
extern u16 dbg_camx;   /* +44/+46: room.c owns live */
extern u16 dbg_camy;
#endif

int main(void)
{
    u16 t;
    u16 pad;
    u16 cx, cy;

    setMode(BG_MODE1, 0);
    bgSetDisable(1); /* BG2 parallax comes later */
    bgSetDisable(2); /* BG3 HUD stub lands with Unit C */

    /* vq statics are WRAM garbage until vq_init — vq_install inits BEFORE
     * handing the drain to the lib's VBlank ISR. */
    vq_install();
    rng_seed(0); /* default "SKYF" seed; tests reseed via the mailbox */

    /* boot into the gantry hall, camera at the spawn corner (bottom-left) */
    room_load(0, 0, 288);

#ifdef TEST_BUILD
    /* Zero the whole block, magic LAST: magic means "valid from here on".
     * (room_load already wrote dbg_room/dbg_roomck/dbg_camx/dbg_camy —
     * they are real state, not garbage, so they are NOT re-zeroed.) */
    dbg_frame = 0;
    dbg_px = 0;
    dbg_py = 0;
    dbg_vx = 0;
    dbg_vy = 0;
    dbg_grav = 0;
    dbg_fsm = 0;
    dbg_pblue = 0;
    dbg_pgold = 0;
    dbg_entn = 0;
    dbg_vqovf = 0;
    dbg_warp = 0;
    dbg_vbl_last = 0;
    dbg_vbl_max = 0;
    dbg_flags = 0;
    dbg_vbl_defer = 0;
    dbg_lag = 0;
    dbg_cmd = 0;
    dbg_arg0 = 0;
    dbg_arg1 = 0;
    dbg_vbl_v = 0;
    lag_frame_counter = 0; /* boot/force-blank time is not gameplay lag */
    dbg_magic = 0x51FE;
#endif

    t = 0;
    cx = room_cam_x();
    cy = room_cam_y();
    while (1)
    {
        /* Unit B free camera: d-pad flies 2px/frame (replaced by the player
         * deadzone-follow in Unit C). room_cam_set clamps + streams seams. */
        pad = padsCurrent(0);
        if (pad & KEY_RIGHT)
            cx += 2;
        if ((pad & KEY_LEFT) && cx >= 2)
            cx -= 2;
        if (pad & KEY_DOWN)
            cy += 2;
        if ((pad & KEY_UP) && cy >= 2)
            cy -= 2;
        room_cam_set(cx, cy);
        cx = room_cam_x(); /* re-sync with the clamp */
        cy = room_cam_y();

#ifdef TEST_BUILD
        dbg_poll(); /* Lua test mailbox (dbgcmd.c) — may warp the camera */
        cx = room_cam_x();
        cy = room_cam_y();
        /* room-warp mailbox (+24): Lua writes room id + 0x8000 */
        if (dbg_warp & 0x8000)
        {
            room_load((u8)(dbg_warp & 0xFF), cx, cy);
            cx = room_cam_x();
            cy = room_cam_y();
            dbg_warp = 0; /* ack */
        }
#endif

        WaitForVBlank();
        t++;
#ifdef TEST_BUILD
        dbg_frame = t;                   /* +1 per displayed frame (INV-HW-003) */
        dbg_vqovf = (u8)(dbg_flags & 1); /* INV-HW-002 probe */
        dbg_lag = lag_frame_counter;     /* 60fps gate feed */
#endif
    }
    return 0;
}
