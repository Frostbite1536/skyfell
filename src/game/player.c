#include "src/game/player.h"

#include "src/audio/sound.h"
#include "src/game/entity.h"
#include "src/game/portal.h"
#include "src/game/room.h"
#include "src/game/tuning.h"

extern char obj_chr, obj_chr_end, obj_pal; /* data.asm (mkobj.py sheet) */

#ifdef TEST_BUILD
extern s32 dbg_px; /* dbg.asm +4  */
extern s32 dbg_py; /* dbg.asm +8  */
extern s16 dbg_vx; /* dbg.asm +12 */
extern s16 dbg_vy; /* dbg.asm +14 */
extern u8 dbg_fsm; /* dbg.asm +17 */
#endif

/* --- state (bare WRAM: player_init writes everything, repo convention) --- */
static s32 px, py; /* box top-left, 16.8 */
static s16 vx, vy; /* 8.8 */
static u8 fsm;
static u8 grounded;
static u8 coyote; /* frames left to jump after walking off a ledge */
static u8 jbuf;   /* frames a buffered jump press stays live */
static u8 jheld;  /* B held (for the variable-height cut) */
static u8 face;   /* 0 right, 1 left */
static u8 anim;   /* frame counter for animation */

/* --- the Rift Gun (Phase 2) --- */
static u8 tp_cool;  /* portal re-entry cooldown */
static u8 aiming;   /* R held (reticle shows) */
static u8 aim;      /* 8-way: 0 E,1 NE,2 N,3 NW,4 W,5 SW,6 S,7 SE */
static u8 held_y, held_x, held_a, held_sel; /* edge detectors */

/* aim direction -> shot velocity components (SHOT_SPD per axis; diagonals
 * ride both, the straight-line 4px/f spec applies per axis). FILE-scope
 * static const only (prophet's function-scope-static .data placement bug). */
static const s16 aim_vx[8] = {SHOT_SPD, SHOT_SPD, 0, -SHOT_SPD,
                              -SHOT_SPD, -SHOT_SPD, 0, SHOT_SPD};
static const s16 aim_vy[8] = {0, -SHOT_SPD, -SHOT_SPD, -SHOT_SPD,
                              0, SHOT_SPD, SHOT_SPD, SHOT_SPD};
static const s8 ret_dx[8] = {20, 14, 0, -14, -20, -14, 0, 14};
static const s8 ret_dy[8] = {0, -14, -20, -14, 0, 14, 20, 14};

s16 player_px(void) { return (s16)(px >> 8); }
s16 player_py(void) { return (s16)(py >> 8); }
u8 player_face(void) { return face; }

extern u16 room_map[];   /* the LIVE room (chamram.asm, D-016) — collision
                            reads the map DIRECTLY: the room_attr call chain
                            cost ~40 scanlines/frame across ~25 queries */
extern u16 room01_att[]; /* shared tileset attrs (roomglue asserts) */

static u8 solid(s16 tx, s16 ty)
{
    u16 a;
    /* negative coords wrap huge as u16 -> out of bounds -> solid */
    if ((u16)tx >= 128 || (u16)ty >= 64)
        return 1;
    if (portal_any && (u16)tx >= portal_bx0 && (u16)tx <= portal_bx1 &&
        (u16)ty >= portal_by0 && (u16)ty <= portal_by1 &&
        portal_cell((u16)tx, (u16)ty))
        return 0; /* an open rift is walk-through */
    a = room01_att[room_map[(u16)(((u16)ty << 7) + (u16)tx)] & 0x3FF];
    return (u8)(ATTR_COL(a) != COL_EMPTY);
}

/* any solid tile across the box's horizontal span at tile row ty? */
static u8 solid_row(s16 x, s16 ty)
{
    s16 tx0 = x >> 3;
    s16 tx1 = (s16)(x + PB_W - 1) >> 3;
    s16 t;
    for (t = tx0; t <= tx1; t++)
        if (solid(t, ty))
            return 1;
    return 0;
}

/* any solid tile across the box's vertical span at tile column tx? */
static u8 solid_col(s16 tx, s16 y)
{
    s16 ty0 = y >> 3;
    s16 ty1 = (s16)(y + PB_H - 1) >> 3;
    s16 t;
    for (t = ty0; t <= ty1; t++)
        if (solid(tx, t))
            return 1;
    return 0;
}

/* Rebase the OBJ character data (force-blank only): rooms keep it at word
 * $2000; the Mode 7 chamber owns words $0000-$3FFF, so it rebases to $4000.
 * OBSEL name base = words>>13; sizes 16x16 small / 32x32 large. */
void player_obj_base(u16 vram_words)
{
    u16 i;
    REG_OBSEL = (u8)((vram_words >> 13) | OBJ_SIZE16_L32);
    dmaCopyVram((u8 *)&obj_chr, vram_words, (u16)(&obj_chr_end - &obj_chr));

    /* hide all 128 sprites (y below the field), high table all-small/x8=0 */
    for (i = 0; i < 512; i += 4)
    {
        oamMemory[i] = 0;
        oamMemory[i + 1] = 0xF0;
        oamMemory[i + 2] = 0;
        oamMemory[i + 3] = 0;
    }
    for (i = 512; i < 544; i++)
        oamMemory[i] = 0;
}

void player_obj_init(void)
{
    player_obj_base(0x2000);
    dmaCopyCGram((u8 *)&obj_pal, 128, 32); /* OBJ palette 0 (once: CGRAM
                                              128+ survives chamber loads,
                                              INV-HW-006) */
}

void player_init(u16 x, u16 y)
{
    px = (s32)x << 8;
    py = (s32)y << 8;
    vx = 0;
    vy = 0;
    fsm = PF_FALL; /* settles to idle on the first grounded frame */
    grounded = 0;
    coyote = 0;
    jbuf = 0;
    jheld = 0;
    face = 0;
    anim = 0;
    tp_cool = 0;
    aiming = 0;
    aim = 0;
    held_y = 0;
    held_x = 0;
    held_a = 0;
    held_sel = 0;
#ifdef TEST_BUILD
    dbg_px = px;
    dbg_py = py;
    dbg_vx = 0;
    dbg_vy = 0;
    dbg_fsm = fsm;
#endif
}

/* deadzone-follow: nudge the camera only when the box center leaves the
 * dead window; per-frame delta capped at CAM_EASE_MAX so a teleport snaps
 * with a short ease instead of demanding a whole-window stream in one
 * frame. room_cam_set clamps to the room edges and streams seams. */
static void cam_follow(void)
{
    s16 ox = (s16)room_cam_x();
    s16 oy = (s16)room_cam_y();
    s16 cx = ox;
    s16 cy = oy;
    s16 pcx = (s16)((s16)(px >> 8) + (PB_W / 2));
    s16 pcy = (s16)((s16)(py >> 8) + (PB_H / 2));
    s16 d;
    d = (s16)(pcx - cx);
    if (d < CAM_DEAD_X0)
        cx = (s16)(pcx - CAM_DEAD_X0);
    else if (d > CAM_DEAD_X1)
        cx = (s16)(pcx - CAM_DEAD_X1);
    d = (s16)(pcy - cy);
    if (grounded)
    {
        if (d < CAM_DEAD_Y0)
            cy = (s16)(pcy - CAM_DEAD_Y0);
        else if (d > CAM_DEAD_Y1)
            cy = (s16)(pcy - CAM_DEAD_Y1);
    }
    else
    {
        /* airborne: hold the line unless Wren nears a screen edge */
        if (d < CAM_AIR_Y0)
            cy = (s16)(pcy - CAM_AIR_Y0);
        else if (d > CAM_AIR_Y1)
            cy = (s16)(pcy - CAM_AIR_Y1);
    }
    if (cx < 0)
        cx = 0;
    if (cy < 0)
        cy = 0;
    if ((s16)(cx - ox) > CAM_EASE_X)
        cx = (s16)(ox + CAM_EASE_X);
    else if ((s16)(cx - ox) < -CAM_EASE_X)
        cx = (s16)(ox - CAM_EASE_X);
    if ((s16)(cy - oy) > CAM_EASE_Y)
        cy = (s16)(oy + CAM_EASE_Y);
    else if ((s16)(cy - oy) < -CAM_EASE_Y)
        cy = (s16)(oy - CAM_EASE_Y);
    room_cam_set((u16)cx, (u16)cy);
}

void player_warp(u16 x, u16 y)
{
    s16 cx, cy;
    player_init(x, y);
    cx = (s16)(x + (PB_W / 2) - 128);
    cy = (s16)(y + (PB_H / 2) - 112);
    if (cx < 0)
        cx = 0;
    if (cy < 0)
        cy = 0;
    room_cam_warp((u16)cx, (u16)cy);
}

/* spawn a portal shot from the box center along the current aim */
static void fire_shot(u8 type)
{
    u8 s = ent_spawn(type,
                     (u16)((s16)(px >> 8) + (PB_W >> 1) - (SHOT_BOX >> 1)),
                     (u16)((s16)(py >> 8) + 8));
    if (s != 0xFF)
    {
        ent_set_vel(s, aim_vx[aim], aim_vy[aim]);
        sfx_play(SFX_FIRE);
    }
}

void player_update(u16 pad)
{
    s16 x, y;
    s16 t;
    u8 press_b = 0;
    u8 oncrate;
    u8 wasg;
    u8 tp;

    aiming = (u8)((pad & KEY_R) ? 1 : 0);

    /* --- input: run (aim-lock: while R is held the d-pad ONLY aims —
     * Jeremy's playtest 2026-07-12: drifting while lining up a corner shot
     * felt wrong) --- */
    if (!aiming && (pad & KEY_RIGHT))
    {
        vx += P_WALK_ACC;
        if (vx > P_WALK_MAX)
            vx = P_WALK_MAX;
        face = 0;
    }
    else if (!aiming && (pad & KEY_LEFT))
    {
        vx -= P_WALK_ACC;
        if (vx < -P_WALK_MAX)
            vx = -P_WALK_MAX;
        face = 1;
    }
    else if (grounded)
    {
        /* friction toward zero — GROUND only (air keeps momentum: portals
         * conserve |v|, the GDD's first pillar). Branch per sign, never
         * multiply (tcc816). */
        if (vx > 0)
        {
            vx -= P_FRICTION;
            if (vx < 0)
                vx = 0;
        }
        else if (vx < 0)
        {
            vx += P_FRICTION;
            if (vx > 0)
                vx = 0;
        }
    }

    /* --- input: jump edge + buffer --- */
    if (pad & KEY_B)
    {
        if (!jheld)
            press_b = 1;
        jheld = 1;
    }
    else
    {
        /* variable height: releasing B while rising cuts the climb.
         * Negate-shift-negate: no signed right shift (tcc816 caution). */
        if (jheld && vy < 0)
            vy = (s16)(-((s16)((s16)(-vy) >> 1)));
        jheld = 0;
    }
    if (press_b)
        jbuf = P_JBUF_F;
    else if (jbuf)
        jbuf--;

    if (jbuf && (grounded || coyote))
    {
        vy = P_JUMP_VY;
        grounded = 0;
        coyote = 0;
        jbuf = 0;
        sfx_play(SFX_JUMP);
    }

    /* --- gravity --- */
    if (!grounded)
    {
        vy += P_GRAV;
        if (vy > P_TERM_VY)
            vy = P_TERM_VY;
    }

    /* --- movement, one axis at a time; the portal transit check runs on
     * each axis BETWEEN motion and the solid sweep (leading-edge semantics:
     * the opening is 8px deep with solid behind it — clamping first would
     * pin a tall box before its edge crosses the plane) --- */
    portal_cool(&tp_cool);
    tp = 0;
    px += vx;
    if (portal_check(0, &px, &py, &vx, &vy, &tp_cool, PB_W, PB_H, 0xFF))
        tp = 1; /* rode a wall rift: position+velocity rewritten */
    else
    {
        x = (s16)(px >> 8);
        y = (s16)(py >> 8);
        if (vx > 0)
        {
            t = (s16)((s16)(x + PB_W - 1) >> 3);
            if (solid_col(t, y))
            {
                x = (s16)((t << 3) - PB_W);
                px = (s32)x << 8;
                vx = 0;
            }
        }
        else if (vx < 0)
        {
            t = (s16)(x >> 3);
            if (solid_col(t, y))
            {
                x = (s16)((t + 1) << 3);
                px = (s32)x << 8;
                vx = 0;
            }
        }
        /* crates are pushable walls (entity.c clamps + imparts push) */
        ent_clamp_x(&px, &vx, (s16)(py >> 8), PB_W, PB_H, grounded);
    }

    oncrate = 0;
    if (!tp)
    {
        py += vy;
        if (portal_check(1, &px, &py, &vx, &vy, &tp_cool, PB_W, PB_H, 0xFF))
            tp = 1; /* rode a floor/ceiling rift */
        else
        {
            x = (s16)(px >> 8);
            y = (s16)(py >> 8);
            if (vy > 0)
            {
                t = (s16)((s16)(y + PB_H - 1) >> 3);
                if (solid_row(x, t))
                {
                    y = (s16)((t << 3) - PB_H);
                    py = (s32)y << 8;
                    vy = 0;
                }
            }
            else if (vy < 0)
            {
                t = (s16)(y >> 3);
                if (solid_row(x, t))
                {
                    y = (s16)((t + 1) << 3);
                    py = (s32)y << 8;
                    vy = 0;
                }
            }
            /* landing on / bonking a crate */
            oncrate = ent_clamp_y(&py, &vy, (s16)(px >> 8), PB_W, PB_H);
        }
    }

    /* --- ground probe (one px below the feet) + coyote --- */
    x = (s16)(px >> 8);
    y = (s16)(py >> 8);
    wasg = grounded;
    grounded = (u8)(oncrate ||
                    (vy >= 0 && solid_row(x, (s16)((s16)(y + PB_H) >> 3))));
    if (grounded && !wasg)
        sfx_play(SFX_LAND); /* air -> ground edge (D-020) */
    if (grounded)
        coyote = P_COYOTE_F;
    else if (coyote)
        coyote--;

    /* --- the Rift Gun --- */
    if (pad & KEY_R)
    {
        /* 8-way aim lock while R is held */
        if (pad & KEY_RIGHT)
        {
            if (pad & KEY_UP)
                aim = 1;
            else if (pad & KEY_DOWN)
                aim = 7;
            else
                aim = 0;
        }
        else if (pad & KEY_LEFT)
        {
            if (pad & KEY_UP)
                aim = 3;
            else if (pad & KEY_DOWN)
                aim = 5;
            else
                aim = 4;
        }
        else if (pad & KEY_UP)
            aim = 2;
        else if (pad & KEY_DOWN)
            aim = 6;
        if (aim == 0 || aim == 1 || aim == 7)
            face = 0; /* facing follows a horizontal-ish aim */
        else if (aim >= 3 && aim <= 5)
            face = 1;
    }
    else
        aim = face ? 4 : 0; /* unlocked: aim rides the facing */


    if (pad & KEY_SELECT)
    {
        if (!held_sel)
            portal_clear(); /* recall both */
        held_sel = 1;
    }
    else
        held_sel = 0;

    if (pad & KEY_A)
    {
        if (!held_a)
            ent_grab_toggle(); /* grab / throw a crate */
        held_a = 1;
    }
    else
        held_a = 0;

    /* two fire buttons — Y blue, X gold (playtest 2026-07-12: the
     * fire+toggle scheme kept re-firing one color; direct buttons are
     * discoverable) */
    if (pad & KEY_Y)
    {
        if (!held_y)
            fire_shot(ET_SHOT_B);
        held_y = 1;
    }
    else
        held_y = 0;
    if (pad & KEY_X)
    {
        if (!held_x)
            fire_shot(ET_SHOT_G);
        held_x = 1;
    }
    else
        held_x = 0;

    /* --- FSM --- */
    if (grounded)
        fsm = (vx != 0) ? PF_RUN : PF_IDLE;
    else
        fsm = (vy < 0) ? PF_JUMP : PF_FALL;

    anim++;
    cam_follow();

#ifdef TEST_BUILD
    dbg_px = px;
    dbg_py = py;
    dbg_vx = vx;
    dbg_vy = vy;
    dbg_fsm = fsm;
#endif
}

void player_render(void)
{
    /* frame select: sheet order idle,idle2,run1..4,jump,fall (mkobj.py) */
    u8 f;
    s16 sx = (s16)((s16)(px >> 8) - PB_SPR_DX - (s16)room_cam_x());
    s16 sy = (s16)((s16)(py >> 8) - PB_SPR_DY - (s16)room_cam_y());
    u8 attr = (u8)(0x20 | (face ? 0x40 : 0)); /* prio 2, pal 0, hflip */

    if (fsm == PF_RUN)
        f = (u8)(2 + ((anim >> 3) & 3));
    else if (fsm == PF_JUMP)
        f = 6;
    else if (fsm == PF_FALL)
        f = 7;
    else
        f = (u8)((anim & 32) ? 1 : 0);

    /* two stacked 16x16 small sprites (OAM 0 top, 1 bottom) — direct shadow
     * stores, prophet's perf lesson (oamSet is a stack-argument call) */
    oamMemory[0] = (u8)sx;
    oamMemory[1] = (u8)sy;
    oamMemory[2] = (u8)(f << 1);
    oamMemory[3] = attr;
    oamMemory[4] = (u8)sx;
    oamMemory[5] = (u8)(sy + 16);
    oamMemory[6] = (u8)(32 + (f << 1));
    oamMemory[7] = attr;

    /* aim reticle (OAM 18, tile 76) 20px out along the 8-way aim */
    if (aiming)
    {
        oamMemory[72] = (u8)(sx + 2 + ret_dx[aim] - 5);
        oamMemory[73] = (u8)(sy + 14 + ret_dy[aim] - 5);
        oamMemory[74] = 76;
        oamMemory[75] = 0x20;
    }
    else
        oamMemory[73] = 0xF0;
}
