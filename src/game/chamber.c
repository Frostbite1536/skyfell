#include "src/game/chamber.h"

#include "src/core/vblank.h"
#include "src/data/generated/chamber.h"
#include "src/game/entity.h"
#include "src/game/player.h"
#include "src/game/portal.h"
#include "src/game/room.h"
#include "src/game/tuning.h"

extern u8 cham_map[];  /* chamber.asm: 128x128 tile bytes */
extern u16 cham_att[]; /* attr per tile id (low col, high mat) */
extern char cham_chr, cham_chr_end, cham_pal;
extern char obj_chr, obj_chr_end; /* data.asm (player_obj_init loads pal) */
extern u16 sin256[]; /* lut.asm, s16 8.8; cos = sin256[(i+64)&255] */

#ifdef TEST_BUILD
extern u16 dbg_frame;
extern s32 dbg_px;
extern s32 dbg_py;
extern s16 dbg_vx;
extern s16 dbg_vy;
extern u8 dbg_grav;
extern u8 dbg_fsm;
extern u8 dbg_room;
extern u8 dbg_roomck;
#endif

/* --- state (chamber_load resets everything; repo convention) --- */
static s32 cpx, cpy;    /* box top-left, 16.8, WORLD coords (0..1023) */
static s16 tv, gv;      /* gravity-frame velocity: tangent / along-gravity */
static u8 grav;         /* 0 down, 1 left, 2 up, 3 right (world dirs) */
static u8 grounded;
static u8 coyote, jbuf, jheld;
static u8 face;         /* 0 = +tangent, 1 = -tangent */
static u8 anim;
static u8 fsm;
static u8 theta;        /* screen rotation angle, 0..255 */
static u8 theta_tgt;
static u8 rotating;     /* physics frozen while easing */

/* gravity unit vector G and tangent T = (Gy, -Gx) per gravity state */
static const s8 g_x[4] = {0, -1, 0, 1};
static const s8 g_y[4] = {1, 0, -1, 0};
static const s8 t_x[4] = {1, 0, -1, 0};
static const s8 t_y[4] = {0, 1, 0, -1};
/* screen angle that puts each gravity at the screen bottom */
static const u8 theta_of[4] = {0, 192, 128, 64};

/* box extent along world axes: upright for vertical gravity, sideways for
 * horizontal (Wren lies along the wall he stands on) */
static u8 box_w(void)
{
    return (grav & 1) ? PB_H : PB_W;
}

static u8 box_h(void)
{
    return (grav & 1) ? PB_W : PB_H;
}

static u8 csolid(s16 tx, s16 ty)
{
    if ((u16)tx >= 128 || (u16)ty >= 128)
        return 1;
    return (u8)((u8)cham_att[cham_map[(u16)(((u16)ty << 7) + (u16)tx)]] != 0);
}

static u8 csolid_row(s16 x, s16 ty, u8 w)
{
    s16 t0 = x >> 3;
    s16 t1 = (s16)(x + w - 1) >> 3;
    s16 t;
    for (t = t0; t <= t1; t++)
        if (csolid(t, ty))
            return 1;
    return 0;
}

static u8 csolid_col(s16 tx, s16 y, u8 h)
{
    s16 t0 = y >> 3;
    s16 t1 = (s16)(y + h - 1) >> 3;
    s16 t;
    for (t = t0; t <= t1; t++)
        if (csolid(tx, t))
            return 1;
    return 0;
}

/* multiply-free +-1/0 component pick */
static s16 mpick(s8 m, s16 v)
{
    if (m > 0)
        return v;
    if (m < 0)
        return (s16)(-v);
    return 0;
}

static void matrix_apply(void)
{
    s16 cs = (s16)sin256[(u8)(theta + 64)];
    s16 sn = (s16)sin256[theta];
    s16 cx = (s16)((s16)(cpx >> 8) + (box_w() >> 1));
    s16 cy = (s16)((s16)(cpy >> 8) + (box_h() >> 1));
    vq_set_m7(cs, sn, (s16)(-sn), cs, cx, cy, (s16)(cx - 128), (s16)(cy - 112));
}

void chamber_set_gravity(u8 g)
{
    grav = (u8)(g & 3);
    theta_tgt = theta_of[grav];
    if (theta != theta_tgt)
        rotating = 1; /* physics freeze; ease in chamber_frame */
    gv = 0;
    tv = 0;
    grounded = 0;
#ifdef TEST_BUILD
    dbg_grav = grav;
#endif
}

void chamber_warp(u16 x, u16 y)
{
    cpx = (s32)x << 8;
    cpy = (s32)y << 8;
    tv = 0;
    gv = 0;
    grounded = 0;
    matrix_apply();
}

void chamber_load(void)
{
    u16 i;
    u16 n;

    setScreenOff();
    portal_init();
    ent_clear_all();

    setMode(BG_MODE7, 0);
    *(vuint8 *)0x211A = 0x00; /* M7SEL: wrap (the map's edge is void) */

    /* Mode 7 VRAM: map = low bytes ($2118, VMAIN inc-on-low), chr = high
     * bytes ($2119, inc-on-high). Force-blank DMA via the lib's Mode 7
     * helper (dmacontrol = BBAD<<8 | DMAP). */
    dmaCopyVram7((u8 *)&cham_map, 0, 16384, 0x00, 0x1800);
    dmaCopyVram7((u8 *)&cham_chr, 0, (u16)(&cham_chr_end - &cham_chr),
                 0x80, 0x1900);

    /* chamber palette: colors 0-127 ONLY (sprites own 128-255, INV-HW-006) */
    dmaCopyCGram((u8 *)&cham_pal, 0, 256);

    /* OBJ tiles rebase to word $4000 (Mode 7 owns $0000-$3FFF) */
    player_obj_base(0x4000);

    /* spawn on the arena floor, gravity down */
    grav = 0;
    theta = 0;
    theta_tgt = 0;
    rotating = 0;
    coyote = 0;
    jbuf = 0;
    jheld = 0;
    face = 0;
    anim = 0;
    fsm = PF_FALL;
    chamber_warp(CHAM_SPAWN_X, CHAM_SPAWN_Y);
    vq_set_m7_on(1);

#ifdef TEST_BUILD
    {
        u16 ck = 0;
        for (i = 0; i < 16384; i++)
            ck += cham_map[i];
        for (i = 0; i < 256; i++)
            ck += cham_att[i];
        dbg_roomck = (ck == CHAM_CKSUM) ? 1 : 2;
        dbg_room = 1;
        dbg_grav = grav;
    }
    n = 0; /* keep tcc quiet in release */
#endif
    (void)n;

    setScreenOn();
    lag_frame_counter = 0;
}

void chamber_frame(u16 pad)
{
    s16 x, y, t;
    s16 wvx, wvy;
    u8 press_b = 0;
    u8 w = box_w();
    u8 h = box_h();

    if (rotating)
    {
        /* eased spin: physics frozen, matrix steps toward the target */
        u8 d = (u8)(theta_tgt - theta);
        if (d == 0)
            rotating = 0;
        else if (d < 128)
            theta = (u8)(theta + ((d < ROT_STEP) ? d : ROT_STEP));
        else
        {
            d = (u8)(theta - theta_tgt);
            theta = (u8)(theta - ((d < ROT_STEP) ? d : ROT_STEP));
        }
        matrix_apply();
        anim++;
#ifdef TEST_BUILD
        dbg_fsm = 5; /* rotating */
#endif
        return;
    }

    /* --- input in the GRAVITY FRAME: left/right run along the tangent --- */
    if (pad & KEY_RIGHT)
    {
        tv += P_WALK_ACC;
        if (tv > P_WALK_MAX)
            tv = P_WALK_MAX;
        face = 0;
    }
    else if (pad & KEY_LEFT)
    {
        tv -= P_WALK_ACC;
        if (tv < -P_WALK_MAX)
            tv = -P_WALK_MAX;
        face = 1;
    }
    else if (grounded)
    {
        if (tv > 0)
        {
            tv -= P_FRICTION;
            if (tv < 0)
                tv = 0;
        }
        else if (tv < 0)
        {
            tv += P_FRICTION;
            if (tv > 0)
                tv = 0;
        }
    }

    if (pad & KEY_B)
    {
        if (!jheld)
            press_b = 1;
        jheld = 1;
    }
    else
    {
        if (jheld && gv < 0)
            gv = (s16)(-((s16)((s16)(-gv) >> 1)));
        jheld = 0;
    }
    if (press_b)
        jbuf = P_JBUF_F;
    else if (jbuf)
        jbuf--;
    if (jbuf && (grounded || coyote))
    {
        gv = P_JUMP_VY;
        grounded = 0;
        coyote = 0;
        jbuf = 0;
    }
    if (!grounded)
    {
        gv += P_GRAV;
        if (gv > P_TERM_VY)
            gv = P_TERM_VY;
    }

    /* gravity frame -> world velocity (components are 0/+-1) */
    wvx = (s16)(mpick(t_x[grav], tv) + mpick(g_x[grav], gv));
    wvy = (s16)(mpick(t_y[grav], tv) + mpick(g_y[grav], gv));

    /* --- X sweep --- */
    cpx += wvx;
    x = (s16)(cpx >> 8);
    y = (s16)(cpy >> 8);
    if (wvx > 0)
    {
        t = (s16)((s16)(x + w - 1) >> 3);
        if (csolid_col(t, y, h))
        {
            x = (s16)((t << 3) - w);
            cpx = (s32)x << 8;
            wvx = 0;
        }
    }
    else if (wvx < 0)
    {
        t = (s16)(x >> 3);
        if (csolid_col(t, y, h))
        {
            x = (s16)((t + 1) << 3);
            cpx = (s32)x << 8;
            wvx = 0;
        }
    }

    /* --- Y sweep --- */
    cpy += wvy;
    x = (s16)(cpx >> 8);
    y = (s16)(cpy >> 8);
    if (wvy > 0)
    {
        t = (s16)((s16)(y + h - 1) >> 3);
        if (csolid_row(x, t, w))
        {
            y = (s16)((t << 3) - h);
            cpy = (s32)y << 8;
            wvy = 0;
        }
    }
    else if (wvy < 0)
    {
        t = (s16)(y >> 3);
        if (csolid_row(x, t, w))
        {
            y = (s16)((t + 1) << 3);
            cpy = (s32)y << 8;
            wvy = 0;
        }
    }

    /* world velocity back into the gravity frame (a collision that zeroed
     * a world axis must zero the matching frame component) */
    tv = (s16)(mpick(t_x[grav], wvx) + mpick(t_y[grav], wvy));
    gv = (s16)(mpick(g_x[grav], wvx) + mpick(g_y[grav], wvy));

    /* --- ground probe: one px beyond the feet face (along G) --- */
    x = (s16)(cpx >> 8);
    y = (s16)(cpy >> 8);
    if (grav == 0)
        grounded = (u8)(gv >= 0 && csolid_row(x, (s16)((s16)(y + h) >> 3), w));
    else if (grav == 2)
        grounded = (u8)(gv >= 0 && csolid_row(x, (s16)((s16)(y - 1) >> 3), w));
    else if (grav == 1)
        grounded = (u8)(gv >= 0 && csolid_col((s16)((s16)(x - 1) >> 3), y, h));
    else
        grounded = (u8)(gv >= 0 && csolid_col((s16)((s16)(x + w) >> 3), y, h));
    if (grounded)
        coyote = P_COYOTE_F;
    else if (coyote)
        coyote--;

    fsm = grounded ? ((tv != 0) ? PF_RUN : PF_IDLE) : ((gv < 0) ? PF_JUMP : PF_FALL);
    anim++;
    matrix_apply();

#ifdef TEST_BUILD
    dbg_px = cpx;
    dbg_py = cpy;
    dbg_vx = wvx;
    dbg_vy = wvy;
    dbg_fsm = fsm;
#endif
}

void chamber_render(void)
{
    /* Wren pinned upright at screen center — the WORLD rotates around him.
     * Frame select mirrors player.c's (sheet order in mkobj.py). */
    u8 f;
    u8 attr = (u8)(0x20 | (face ? 0x40 : 0));
    if (rotating)
        f = 6; /* braced jump pose through the spin */
    else if (fsm == PF_RUN)
        f = (u8)(2 + ((anim >> 3) & 3));
    else if (fsm == PF_JUMP)
        f = 6;
    else if (fsm == PF_FALL)
        f = 7;
    else
        f = (u8)((anim & 32) ? 1 : 0);
    oamMemory[0] = 120;
    oamMemory[1] = 96;
    oamMemory[2] = (u8)(f << 1);
    oamMemory[3] = attr;
    oamMemory[4] = 120;
    oamMemory[5] = 112;
    oamMemory[6] = (u8)(32 + (f << 1));
    oamMemory[7] = attr;
    oamMemory[73] = 0xF0; /* no reticle in the chamber (yet) */
}
