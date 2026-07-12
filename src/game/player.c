#include "src/game/player.h"

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

static u8 solid(s16 tx, s16 ty)
{
    /* negative coords wrap huge as u16 -> out of bounds -> solid */
    return (u8)(ATTR_COL(room_attr((u16)tx, (u16)ty)) != COL_EMPTY);
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

void player_obj_init(void)
{
    u16 i;
    /* OBSEL: OBJ char base word $2000 (8K-word steps -> 0x01), sizes
     * 16x16 small / 32x32 large (we use small). Boot runs force-blank, so
     * the direct PPU writes are legal (INV-HW-001). */
    REG_OBSEL = 0x01 | OBJ_SIZE16_L32;
    dmaCopyVram((u8 *)&obj_chr, 0x2000, (u16)(&obj_chr_end - &obj_chr));
    dmaCopyCGram((u8 *)&obj_pal, 128, 32); /* OBJ palette 0 */

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
#ifdef TEST_BUILD
    dbg_px = px;
    dbg_py = py;
    dbg_vx = 0;
    dbg_vy = 0;
    dbg_fsm = fsm;
#endif
}

/* deadzone-follow: nudge the camera only when the box center leaves the
 * dead window; room_cam_set clamps to the room edges and streams seams */
static void cam_follow(void)
{
    s16 cx = (s16)room_cam_x();
    s16 cy = (s16)room_cam_y();
    s16 pcx = (s16)((s16)(px >> 8) + (PB_W / 2));
    s16 pcy = (s16)((s16)(py >> 8) + (PB_H / 2));
    s16 d;
    d = (s16)(pcx - cx);
    if (d < CAM_DEAD_X0)
        cx = (s16)(pcx - CAM_DEAD_X0);
    else if (d > CAM_DEAD_X1)
        cx = (s16)(pcx - CAM_DEAD_X1);
    d = (s16)(pcy - cy);
    if (d < CAM_DEAD_Y0)
        cy = (s16)(pcy - CAM_DEAD_Y0);
    else if (d > CAM_DEAD_Y1)
        cy = (s16)(pcy - CAM_DEAD_Y1);
    if (cx < 0)
        cx = 0;
    if (cy < 0)
        cy = 0;
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

void player_update(u16 pad)
{
    s16 x, y;
    s16 t;
    u8 press_b = 0;

    /* --- input: run --- */
    if (pad & KEY_RIGHT)
    {
        vx += P_WALK_ACC;
        if (vx > P_WALK_MAX)
            vx = P_WALK_MAX;
        face = 0;
    }
    else if (pad & KEY_LEFT)
    {
        vx -= P_WALK_ACC;
        if (vx < -P_WALK_MAX)
            vx = -P_WALK_MAX;
        face = 1;
    }
    else
    {
        /* friction toward zero — branch per sign, never multiply (tcc816) */
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
    }

    /* --- gravity --- */
    if (!grounded)
    {
        vy += P_GRAV;
        if (vy > P_TERM_VY)
            vy = P_TERM_VY;
    }

    /* --- X sweep (max speed < 8px/f, so one tile boundary per frame) --- */
    px += vx;
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

    /* --- Y sweep --- */
    py += vy;
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

    /* --- ground probe (one px below the feet) + coyote --- */
    y = (s16)(py >> 8);
    grounded = (u8)(vy >= 0 && solid_row(x, (s16)((s16)(y + PB_H) >> 3)));
    if (grounded)
        coyote = P_COYOTE_F;
    else if (coyote)
        coyote--;

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
}
