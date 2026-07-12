/* RIFT: THE SKYFELL ENGINE — Phase 1: the platformer core.
 *
 * Frame (ARCHITECTURE.md): read pad -> player FSM/physics/collision ->
 * camera deadzone-follow (streams BG1 seams through the vblank queue,
 * INV-HW-001) -> OAM shadow -> WaitForVBlank; the NMI drains the queue and
 * DMAs OAM. Fixed point s32 16.8 / s16 8.8 everywhere (INV-ENG-001);
 * every knob in src/game/tuning.h. */

#include <snes.h>

#include "src/core/dbgcmd.h"
#include "src/core/rng.h"
#include "src/core/vblank.h"
#include "src/game/chamber.h"
#include "src/game/entity.h"
#include "src/game/player.h"
#include "src/game/portal.h"
#include "src/game/room.h"
#include "src/game/tuning.h"

u8 game_mode; /* 0 = Mode 1 room, 1 = the Mode 7 chamber (dbgcmd routes
                 POS_SET by this) */

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
extern u16 dbg_tpcount; /* +48..+60: Phase 2 (portal.c/entity.c own live) */
extern u16 dbg_e0x;
extern u16 dbg_e0y;
extern u16 dbg_e0vx;
extern u16 dbg_e0vy;
extern u16 dbg_ewatch;
extern u16 dbg_fizz;
extern u16 dbg_mainv;
extern u16 dbg_prof0;
extern u16 dbg_prof1;
extern u16 dbg_exit; /* +68: puzzle exit reached (chamber.c) */
#endif

int main(void)
{
    u16 t;
    u16 pad;

    setMode(BG_MODE1, 0);
    bgSetDisable(1); /* BG2 parallax comes later */
    bgSetDisable(2); /* BG3 HUD stub comes with the first real HUD need */

    /* vq statics are WRAM garbage until vq_init — vq_install inits BEFORE
     * handing the drain to the lib's VBlank ISR. */
    vq_install();
    rng_seed(0); /* default "SKYF" seed; tests reseed via the mailbox */

    /* OBJ chr/pal + OAM hide-all under boot force-blank, then the room
     * (room_load ends with setScreenOn). portal_init BEFORE room_load:
     * the map overlay consults portal state during the window draw. */
    game_mode = 0;
    player_obj_init();
    portal_init();
    portal_world = 0;
    room_load(0, 0, 288); /* camera lands at the spawn corner */
    player_init(SPAWN_X, SPAWN_Y);
    ent_room_init(0);

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
    dbg_tpcount = 0;
    dbg_e0x = 0;
    dbg_e0y = 0;
    dbg_e0vx = 0;
    dbg_e0vy = 0;
    dbg_ewatch = 0x00FF; /* no slot watched */
    dbg_fizz = 0;
    dbg_mainv = 0;
    dbg_exit = 0;
    dbg_magic = 0x51FE;
#endif

    /* absorb the partially-elapsed boot frame so lag==0 means the LIVE loop
     * holds 60fps from its first full frame */
    WaitForVBlank();
    lag_frame_counter = 0;

    t = 0;
    while (1)
    {
        pad = padsCurrent(0);
        if (game_mode)
        {
            chamber_frame(pad); /* gravity-frame physics + rotation SM */
#ifdef TEST_BUILD
            dbg_prof0 = vq_scanline();
#endif
            if (!cham_rot)
                ent_update(); /* the tween freezes the WORLD: entities too
                                 (they're hidden — sprites can't rotate) */
            chamber_render();
            ent_render();
        }
        else
        {
            player_update(pad); /* physics + collision + camera follow */
#ifdef TEST_BUILD
            dbg_prof0 = vq_scanline(); /* stage bracket: after player */
#endif
            ent_update();    /* crates, sentries, shots (portal transits) */
            player_render(); /* OAM shadow; the lib ISR DMAs it */
            ent_render();
        }
#ifdef TEST_BUILD
        dbg_prof1 = vq_scanline(); /* stage bracket: after entities+render */
#endif

#ifdef TEST_BUILD
        dbg_poll(); /* Lua test mailbox (dbgcmd.c) — may warp the player */
        /* room-warp mailbox (+24): Lua writes room id + 0x8000.
         * Room 1 = THE CHAMBER (Mode 7); anything else = the gantry hall. */
        if (dbg_warp & 0x8000)
        {
            if ((u8)(dbg_warp & 0xFF) == 1)
            {
                game_mode = 1;
                chamber_load();
            }
            else
            {
                game_mode = 0;
                setScreenOff(); /* PPU regs below need blank (INV-HW-001) */
                vq_set_m7_on(0);
                portal_init();
                portal_world = 0;
                setMode(BG_MODE1, 0);
                bgSetDisable(1);
                bgSetDisable(2);
                player_obj_base(0x2000);
                room_load((u8)(dbg_warp & 0xFF), room_cam_x(), room_cam_y());
                player_warp(SPAWN_X, SPAWN_Y);
                ent_room_init((u8)(dbg_warp & 0xFF));
            }
            dbg_warp = 0; /* ack */
        }
        dbg_mainv = vq_scanline(); /* frame-cost probe: where work ended */
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
