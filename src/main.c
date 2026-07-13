/* RIFT: THE SKYFELL ENGINE — Phase 1: the platformer core.
 *
 * Frame (ARCHITECTURE.md): read pad -> player FSM/physics/collision ->
 * camera deadzone-follow (streams BG1 seams through the vblank queue,
 * INV-HW-001) -> OAM shadow -> WaitForVBlank; the NMI drains the queue and
 * DMAs OAM. Fixed point s32 16.8 / s16 8.8 everywhere (INV-ENG-001);
 * every knob in src/game/tuning.h. */

#include <snes.h>

#include "src/audio/sound.h"
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

/* --- door transitions (D-017): stand in a doorway + press UP (up-gated
 * so no scripted test path can trip one by walking); fade out via the
 * NMI brightness shadow, force-blank load, fade in. Links hand-tabled
 * until Zone 1 authoring (D-016 convention). Room id 1 = the chamber. */
static u8 cur_room_id; /* current Mode 1 room while game_mode==0 */
static u8 fade;        /* 0 live, 1 fading out, 2 fading in */
static u8 fade_lvl;    /* 0..15 */
static u8 fade_dst;
static u16 fade_px, fade_py;
static u8 held_up;

/* {room, x0, x1, y0, y1, target, entry x, entry y} — px, inclusive */
#define DOOR_N 3
static const u16 door_tab[DOOR_N][8] = {
    {0, 16, 31, 416, 447, 2, 960, 434},   /* hall west door -> room02 */
    {0, 608, 639, 448, 479, 1, 0, 0},     /* the pit door -> the chamber */
    {2, 992, 1007, 432, 463, 0, 40, 418}, /* room02 east door -> the hall */
};

/* per-room default entry (hand-tabled per D-016; used by the TEST warp
 * mailbox and the death respawn) */
static u16 room_entry_x(u8 id) { return (id == 2) ? 88 : SPAWN_X; }
static u16 room_entry_y(u8 id) { return (id == 2) ? 434 : SPAWN_Y; }

/* --- title + end-of-demo card (D-019): pvsneslib console text on a
 * force-blanked repoint of BG1; the NMI uploads it while vq_console is
 * set. Release boots into the title; TEST builds reach it via verb 10
 * (the boot path must not stall the harness). START dismisses into the
 * hall through goto_room, which reloads everything the card clobbered. */
extern char tilfont, palfont; /* data.asm (gfx4snes font) */
u8 in_title; /* 0 live, 1 title, 2 end card (dbgcmd reads none) */

void game_title(u8 kind)
{
    u16 i;
    setScreenOff();
    consoleSetTextMapPtr(0x6800); /* Phase 0's proven console layout */
    consoleSetTextGfxPtr(0x3000);
    consoleSetTextOffset(0x0100);
    consoleInitText(0, 16 * 2, &tilfont, &palfont);
    bgSetGfxPtr(0, 0x2000);
    bgSetMapPtr(0, 0x6800, SC_32x32);
    setMode(BG_MODE1, 0);
    bgSetDisable(1);
    bgSetDisable(2);
    vq_set_m7_on(0);
    /* the map region and tile 0 at this gfx base hold stale room VRAM —
     * blank both (map -> tile 0, tile 0 -> transparent), hide every
     * sprite, and pin the scroll */
    fb_vram_fill(0x6800, 0x0000, 0x400);
    fb_vram_fill(0x2000, 0x0000, 16);
    for (i = 0; i < 512; i += 4)
        oamMemory[i + 1] = 0xF0;
    vq_set_scroll(0, 0);
    if (kind == 1)
    {
        consoleDrawText(9, 9, "THE GYRE TURNS");
        consoleDrawText(6, 12, "DEMO COMPLETE - THANKS!");
        consoleDrawText(9, 18, "PRESS START");
        in_title = 2;
    }
    else
    {
        consoleDrawText(12, 7, "R I F T");
        consoleDrawText(7, 10, "THE SKYFELL ENGINE");
        consoleDrawText(10, 18, "PRESS START");
        in_title = 1;
    }
    vq_console = 1;
    setScreenOn();
    lag_frame_counter = 0;
}

/* full mode/room switch under force-blank (the TEST warp mailbox and the
 * door fades share this one path) */
static void goto_room(u8 id, u16 x, u16 y)
{
    in_title = 0; /* any room switch dismisses a console screen */
    vq_console = 0;
    sfx_quiet(8); /* the load re-grounds the player — no phantom LAND */
    if (id == 1)
    {
        game_mode = 1;
        chamber_load(); /* its own fixed spawn; x,y unused */
        return;
    }
    game_mode = 0;
    cur_room_id = id;
    setScreenOff(); /* PPU regs below need blank (INV-HW-001) */
    vq_set_m7_on(0);
    portal_init();
    portal_world = 0;
    setMode(BG_MODE1, 0);
    bgSetDisable(1);
    bgSetDisable(2);
    player_obj_base(0x2000);
    room_load(id, room_cam_x(), room_cam_y());
    player_warp(x, y);
    ent_room_init(id);
}

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
extern u16 dbg_exit;  /* +68: puzzle exit reached (chamber.c) */
extern u16 dbg_death; /* +70: deaths since boot */
extern u16 dbg_audio; /* +72: sound.c owns live (D-020) */
extern u16 dbg_sfx;   /* +74: sound.c owns live */
#endif

int main(void)
{
    u16 t;
    u16 pad;

    audio_boot(); /* SPC700 handshake FIRST — NMI is still off here and
                     spcBoot must not be interrupted (D-020) */

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

    /* soundbank -> ARAM (slow: two module transfers + 6 effects) BEFORE the
     * debug magic below — tests gate on the magic, so the load must be done
     * by the time they start counting frames (D-020) */
    audio_start();

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
    dbg_death = 0;
    dbg_sfx = 0; /* dbg_audio NOT re-zeroed: audio_start owns it (like
                    dbg_room — real state, not garbage) */
    dbg_magic = 0x51FE;
#endif

    /* absorb the partially-elapsed boot frame so lag==0 means the LIVE loop
     * holds 60fps from its first full frame */
    WaitForVBlank();
    lag_frame_counter = 0;

    t = 0;
    fade = 0;
    fade_lvl = 15;
    held_up = 0;
    cur_room_id = 0;
    in_title = 0;
#ifndef TEST_BUILD
    game_title(0); /* release boots into the title; TEST boots straight to
                      the hall (the harness must not stall) — verb 10
                      covers the title path under test (D-019) */
#endif
    while (1)
    {
        pad = padsCurrent(0);
        if (in_title)
        {
            /* the world is parked behind the card; START enters the hall
             * (the end card returns beside the pit, like the recess exit) */
            if (pad & KEY_START)
            {
                if (in_title == 2)
                    goto_room(0, 256, 418);
                else
                    goto_room(0, room_entry_x(0), room_entry_y(0));
            }
        }
        else if (fade == 1)
        {
            /* fading out: 2 brightness steps/frame, world frozen */
            if (fade_lvl >= 2)
            {
                fade_lvl = (u8)(fade_lvl - 2);
                vq_set_bright(fade_lvl);
            }
            else
            {
                goto_room(fade_dst, fade_px, fade_py);
                REG_INIDISP = 0; /* the load's setScreenOn restored full
                                    brightness mid-frame — re-darken on the
                                    same boundary frame (fade-in owns the
                                    ramp; INV-HW-001 boundary category) */
                vq_set_bright(0);
                fade = 2;
            }
        }
        else if (fade == 2)
        {
            fade_lvl = (u8)(fade_lvl + 2);
            if (fade_lvl >= 15)
            {
                fade_lvl = 15;
                fade = 0;
            }
            vq_set_bright(fade_lvl);
        }
        else if (game_mode)
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
            if (cham_exit)
            {
                cham_exit = 0;
                game_title(1); /* the demo's arc ends here: the card;
                                  START returns beside the pit (D-019) */
            }
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

            if (ent_hit_player)
            {
                /* death (D-018): fade out, reload the CURRENT room at its
                 * entry — a full reset (portals recalled, entities respawn:
                 * INV-GAME-001's room-reset guarantee) */
                ent_hit_player = 0;
                sfx_play(SFX_DEATH);
                fade = 1;
                fade_dst = cur_room_id;
                fade_px = room_entry_x(cur_room_id);
                fade_py = room_entry_y(cur_room_id);
#ifdef TEST_BUILD
                dbg_death++;
#endif
            }

            /* doors: UP edge while standing in a doorway (R excluded —
             * R+UP is the aim-lock) */
            if ((pad & KEY_UP) && !(pad & KEY_R))
            {
                if (!held_up)
                {
                    u8 di;
                    s16 dpx = player_px();
                    s16 dpy = player_py();
                    for (di = 0; di < DOOR_N; di++)
                    {
                        if ((u8)door_tab[di][0] != cur_room_id)
                            continue;
                        if (dpx >= (s16)(door_tab[di][1] + 1) - PB_W &&
                            dpx <= (s16)door_tab[di][2] &&
                            dpy >= (s16)(door_tab[di][3] + 1) - PB_H &&
                            dpy <= (s16)door_tab[di][4])
                        {
                            fade = 1;
                            fade_dst = (u8)door_tab[di][5];
                            fade_px = door_tab[di][6];
                            fade_py = door_tab[di][7];
                            break;
                        }
                    }
                }
                held_up = 1;
            }
            else
                held_up = 0;
        }
#ifdef TEST_BUILD
        dbg_prof1 = vq_scanline(); /* stage bracket: after entities+render */
#endif

        audio_frame(); /* SNESMOD message pump — inside the dbg_mainv
                          bracket so its cost shows in the frame probe */

#ifdef TEST_BUILD
        dbg_poll(); /* Lua test mailbox (dbgcmd.c) — may warp the player */
        /* room-warp mailbox (+24): Lua writes room id + 0x8000.
         * Room 1 = THE CHAMBER (Mode 7); anything else = the gantry hall. */
        if (dbg_warp & 0x8000)
        {
            u8 rid = (u8)(dbg_warp & 0xFF);
            goto_room(rid, room_entry_x(rid), room_entry_y(rid));
            fade = 0; /* test warps are instant — cancel any fade */
            fade_lvl = 15;
            vq_set_bright(15);
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
