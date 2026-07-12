#include "src/game/portal.h"

#include "src/game/entity.h"
#include "src/game/room.h"
#include "src/game/tuning.h"

#ifdef TEST_BUILD
extern u8 dbg_pblue;   /* dbg.asm +19: bit7 placed, bits0-1 orient */
extern u8 dbg_pgold;   /* dbg.asm +20 */
extern u16 dbg_tpcount; /* dbg.asm +48 */
extern u16 dbg_fizz;   /* dbg.asm +60 */
#endif

/* BG tile ids (tools/tilesdef.py) + map-word flip bits */
#define T_PV_CAP_B 27
#define T_PV_BODY_B 28
#define T_PH_CAP_B 29
#define T_PH_BODY_B 30
#define GOLD_OFF 4 /* gold set = blue set + 4 */
#define W_HFLIP 0x4000
#define W_VFLIP 0x8000

#define STRIP 6 /* 48px opening = 6 tiles (GDD) */

/* --- state --- */
static u8 p_on[2];
static u8 p_or[2];  /* outward normal 0..3 */
static u16 p_tx[2]; /* strip start tile (left/top-most) */
static u16 p_ty[2];
u8 portal_any;      /* p_on[0]|p_on[1] — GLOBAL on purpose: room.c's per-tile
                       hot paths (collision queries, seam streaming) read it
                       inline before paying any function call. Without the
                       early-out the overlay cost 32-64 lag frames across
                       test_room's walk (Phase 2 lag lesson). */
#define p_any portal_any

/* The 16-entry transform LUT (INV-ENG-003) — THE only teleport math.
 * v' = v_n*n_out + v_t*(-t_out); rows generated from n/t bases, verified:
 * fall (0,+5) into a floor portal, out a right-wall portal -> (+5,0).
 * File-scope static const: the tcc816 ROM-table shape prophet validated. */
static const s8 tp_m[16][4] = {
    {-1, 0, 0, -1}, {0, 1, -1, 0}, {1, 0, 0, 1},   {0, -1, 1, 0},  /* in=up */
    {0, -1, 1, 0},  {-1, 0, 0, -1}, {0, 1, -1, 0}, {1, 0, 0, 1},   /* in=right */
    {1, 0, 0, 1},   {0, -1, 1, 0}, {-1, 0, 0, -1}, {0, 1, -1, 0},  /* in=down */
    {0, 1, -1, 0},  {1, 0, 0, 1},  {0, -1, 1, 0},  {-1, 0, 0, -1}, /* in=left */
};

/* outward normal components per orient (0 up, 1 right, 2 down, 3 left) */
static const s8 nx_of[4] = {0, 1, 0, -1};
static const s8 ny_of[4] = {-1, 0, 1, 0};

void portal_fix_row(u16 wy, u16 *buf, u16 wx0)
{
    u8 c, k;
    u16 wx;
    for (c = 0; c < 2; c++)
    {
        if (!p_on[c])
            continue;
        if (p_or[c] == 0 || p_or[c] == 2)
        {
            if (p_ty[c] != wy)
                continue;
            for (k = 0; k < STRIP; k++)
            {
                wx = (u16)(p_tx[c] + k);
                if ((u16)(wx - wx0) < 64)
                    buf[wx & 63] = portal_map_word(wx, wy);
            }
        }
        else
        {
            if (wy < p_ty[c] || wy >= (u16)(p_ty[c] + STRIP))
                continue;
            wx = p_tx[c];
            if ((u16)(wx - wx0) < 64)
                buf[wx & 63] = portal_map_word(wx, wy);
        }
    }
}

void portal_fix_col(u16 wx, u16 *buf, u16 wy0)
{
    u8 c, k;
    u16 wy;
    for (c = 0; c < 2; c++)
    {
        if (!p_on[c])
            continue;
        if (p_or[c] == 1 || p_or[c] == 3)
        {
            if (p_tx[c] != wx)
                continue;
            for (k = 0; k < STRIP; k++)
            {
                wy = (u16)(p_ty[c] + k);
                if ((u16)(wy - wy0) < 32)
                    buf[wy & 31] = portal_map_word(wx, wy);
            }
        }
        else
        {
            /* horizontal strip: crosses this column at exactly one row */
            if (wx >= p_tx[c] && wx < (u16)(p_tx[c] + STRIP))
            {
                wy = p_ty[c];
                if ((u16)(wy - wy0) < 32)
                    buf[wy & 31] = portal_map_word(wx, wy);
            }
        }
    }
}

#ifdef TEST_BUILD
static void dbg_mirror(void)
{
    dbg_pblue = (u8)((p_on[0] ? 0x80 : 0) | p_or[0]);
    dbg_pgold = (u8)((p_on[1] ? 0x80 : 0) | p_or[1]);
}
#endif

void portal_init(void)
{
    p_on[0] = 0;
    p_on[1] = 0;
    p_or[0] = 0;
    p_or[1] = 0;
    p_tx[0] = 0;
    p_tx[1] = 0;
    p_ty[0] = 0;
    p_ty[1] = 0;
    p_any = 0;
#ifdef TEST_BUILD
    dbg_mirror();
#endif
}

u8 portal_on(u8 color)
{
    return p_on[color];
}

static u8 horiz(u8 o)
{
    return (u8)(o == 0 || o == 2); /* floor/ceiling: strip runs along x */
}

u8 portal_cell(u16 tx, u16 ty)
{
    u8 c;
    if (!p_any)
        return 0;
    for (c = 0; c < 2; c++)
    {
        if (!p_on[c])
            continue;
        if (horiz(p_or[c]))
        {
            if (ty == p_ty[c] && tx >= p_tx[c] && tx < (u16)(p_tx[c] + STRIP))
                return 1;
        }
        else
        {
            if (tx == p_tx[c] && ty >= p_ty[c] && ty < (u16)(p_ty[c] + STRIP))
                return 1;
        }
    }
    return 0;
}

u16 portal_map_word(u16 tx, u16 ty)
{
    u8 c;
    u16 k;
    u16 base;
    u16 w;
    if (!p_any)
        return 0;
    for (c = 0; c < 2; c++)
    {
        if (!p_on[c])
            continue;
        base = (c == PC_GOLD) ? GOLD_OFF : 0;
        if (horiz(p_or[c]))
        {
            if (ty != p_ty[c] || tx < p_tx[c] || tx >= (u16)(p_tx[c] + STRIP))
                continue;
            k = tx - p_tx[c];
            if (k == 0)
                w = (u16)(T_PH_CAP_B + base);
            else if (k == STRIP - 1)
                w = (u16)(T_PH_CAP_B + base) | W_HFLIP;
            else
                w = (u16)(T_PH_BODY_B + base);
            if (p_or[c] == 2)
                w ^= W_VFLIP; /* ceiling: slit hugs the bottom edge */
            return w;
        }
        else
        {
            if (tx != p_tx[c] || ty < p_ty[c] || ty >= (u16)(p_ty[c] + STRIP))
                continue;
            k = ty - p_ty[c];
            if (k == 0)
                w = (u16)(T_PV_CAP_B + base);
            else if (k == STRIP - 1)
                w = (u16)(T_PV_CAP_B + base) | W_VFLIP;
            else
                w = (u16)(T_PV_BODY_B + base);
            if (p_or[c] == 1)
                w ^= W_HFLIP; /* right-facing: slit hugs the right edge */
            return w;
        }
    }
    return 0;
}

/* refresh a portal's BG cells (place/remove) through the vblank queue */
static void refresh(u8 c)
{
    if (horiz(p_or[c]))
        room_refresh_strip(p_tx[c], p_ty[c], STRIP, 0);
    else
        room_refresh_strip(p_tx[c], p_ty[c], STRIP, 1);
}

void portal_clear(void)
{
    u8 c;
    for (c = 0; c < 2; c++)
    {
        if (p_on[c])
        {
            p_on[c] = 0;
            refresh(c); /* p_on already 0: cells redraw as raw room */
        }
    }
    p_any = 0;
    ent_wake_all(); /* solidity restored under any crate in an opening */
#ifdef TEST_BUILD
    dbg_mirror();
#endif
}

/* THE validator (INV-ENG-004): placement and teleport both call this.
 * Strip start (sx,sy), running +x when horizontal else +y. Every strip
 * tile must be solid BRASS; every front tile (strip + outward normal)
 * must be collision-empty and entity-free; no overlap with the other
 * portal's strip. `self` = color being validated (its own current cells
 * read as raw brass already since attrs come from the raw map). */
static u8 strip_valid(u8 self, u16 sx, u16 sy, u8 orient)
{
    u8 k;
    u16 tx, ty, fx, fy;
    u16 a;
    u8 other = (u8)(self ^ 1);
    s8 nx = nx_of[orient];
    s8 ny = ny_of[orient];
    u8 hz = horiz(orient);
    for (k = 0; k < STRIP; k++)
    {
        tx = hz ? (u16)(sx + k) : sx;
        ty = hz ? sy : (u16)(sy + k);
        fx = (u16)(tx + nx);
        fy = (u16)(ty + ny);
        a = room_attr_raw(tx, ty);
        if (ATTR_COL(a) == COL_EMPTY || ATTR_MAT(a) != MAT_BRASS)
            return 0;
        if (ATTR_COL(room_attr_raw(fx, fy)) != COL_EMPTY)
            return 0;
        if (p_on[other])
        {
            /* the other portal may not share strip or front cells */
            if (portal_cell(tx, ty) || portal_cell(fx, fy))
                return 0;
        }
        if (ent_occupies_tile(tx, ty) || ent_occupies_tile(fx, fy))
            return 0;
    }
    return 1;
}

u8 portal_try_place(u8 color, u16 tx, u16 ty, u8 orient)
{
    static const s8 slide[5] = {0, -1, 1, -2, 2};
    u8 s;
    u8 was_on = p_on[color];
    u16 old_tx = p_tx[color];
    u16 old_ty = p_ty[color];
    u8 old_or = p_or[color];
    u16 sx, sy;
    for (s = 0; s < 5; s++)
    {
        if (horiz(orient))
        {
            sx = (u16)(tx - 2 + slide[s]);
            sy = ty;
        }
        else
        {
            sx = tx;
            sy = (u16)(ty - 2 + slide[s]);
        }
        if (strip_valid(color, sx, sy, orient))
        {
            /* commit: erase the old cells first (refiring moves it) */
            if (was_on)
            {
                p_on[color] = 0;
                p_tx[color] = old_tx;
                p_ty[color] = old_ty;
                p_or[color] = old_or;
                refresh(color);
            }
            p_on[color] = 1;
            p_any = 1;
            p_tx[color] = sx;
            p_ty[color] = sy;
            p_or[color] = orient;
            refresh(color);
            ent_wake_all(); /* the world's solidity changed under crates */
#ifdef TEST_BUILD
            dbg_mirror();
#endif
            return 1;
        }
    }
#ifdef TEST_BUILD
    dbg_fizz++;
#endif
    return 0;
}

/* multiply-free +-1/0 MAC term (tcc816: no runtime multiply needed) */
static s16 mterm(s8 m, s16 v)
{
    if (m > 0)
        return v;
    if (m < 0)
        return (s16)(-v);
    return 0;
}

u8 portal_check(s32 *mx, s32 *my, s16 *mvx, s16 *mvy, u8 *cool, u8 w, u8 h)
{
    u8 c, o, in_or, out_or, k;
    s16 cx, cy;
    s16 zx0, zy0;
    s16 d;
    s16 icx, icy, ocx, ocy; /* in/out portal center points (on the plane) */
    s16 nvx, nvy;
    s16 push;
    s16 ecx, ecy;

    if (*cool)
    {
        (*cool)--;
        return 0;
    }
    if (!p_on[0] || !p_on[1])
        return 0;

    cx = (s16)((s16)(*mx >> 8) + (w >> 1));
    cy = (s16)((s16)(*my >> 8) + (h >> 1));

    for (c = 0; c < 2; c++)
    {
        in_or = p_or[c];
        zx0 = (s16)(p_tx[c] << 3);
        zy0 = (s16)(p_ty[c] << 3);
        if (horiz(in_or))
        {
            if (cx < zx0 || cx >= (s16)(zx0 + 48) || cy < zy0 || cy >= (s16)(zy0 + 8))
                continue;
            if (in_or == 0 && *mvy <= 0)
                continue; /* floor portal: must be moving down into it */
            if (in_or == 2 && *mvy >= 0)
                continue;
        }
        else
        {
            if (cx < zx0 || cx >= (s16)(zx0 + 8) || cy < zy0 || cy >= (s16)(zy0 + 48))
                continue;
            if (in_or == 1 && *mvx >= 0)
                continue; /* right-facing wall: enter moving left */
            if (in_or == 3 && *mvx <= 0)
                continue;
        }

        /* re-validate BOTH portals at teleport time (INV-ENG-004) */
        if (!strip_valid(c, p_tx[c], p_ty[c], p_or[c]))
            return 0;
        o = (u8)(c ^ 1);
        if (!strip_valid(o, p_tx[o], p_ty[o], p_or[o]))
            return 0;
        out_or = p_or[o];

        /* portal center points on their planes */
        if (horiz(in_or))
        {
            icx = (s16)(zx0 + 24);
            icy = (s16)(zy0 + ((in_or == 0) ? 0 : 8));
        }
        else
        {
            icx = (s16)(zx0 + ((in_or == 3) ? 0 : 8));
            icy = (s16)(zy0 + 24);
        }
        if (horiz(out_or))
        {
            ocx = (s16)((s16)(p_tx[o] << 3) + 24);
            ocy = (s16)((s16)(p_ty[o] << 3) + ((out_or == 0) ? 0 : 8));
        }
        else
        {
            ocx = (s16)((s16)(p_tx[o] << 3) + ((out_or == 3) ? 0 : 8));
            ocy = (s16)((s16)(p_ty[o] << 3) + 24);
        }

        /* tangential entry offset d = (center - in_center) . t_in,
         * t(o) = (-n.y, n.x) */
        if (in_or == 0)
            d = (s16)(cx - icx);
        else if (in_or == 1)
            d = (s16)(cy - icy);
        else if (in_or == 2)
            d = (s16)(icx - cx);
        else
            d = (s16)(icy - cy);

        /* exit center = out_center + n_out*push + t_out*(-d) */
        push = (s16)(((horiz(out_or) ? h : w) >> 1) + 3);
        ecx = ocx;
        ecy = ocy;
        if (out_or == 0)
        {
            ecy -= push;
            ecx -= d; /* t_out=(1,0): + (-d) */
        }
        else if (out_or == 1)
        {
            ecx += push;
            ecy -= d;
        }
        else if (out_or == 2)
        {
            ecy += push;
            ecx += d;
        }
        else
        {
            ecx -= push;
            ecy += d;
        }

        /* velocity through the LUT */
        k = (u8)((in_or << 2) | out_or);
        nvx = (s16)(mterm(tp_m[k][0], *mvx) + mterm(tp_m[k][1], *mvy));
        nvy = (s16)(mterm(tp_m[k][2], *mvx) + mterm(tp_m[k][3], *mvy));

        /* speed cap 6 px/f per axis (GDD) */
        if (nvx > TP_SPEED_CAP)
            nvx = TP_SPEED_CAP;
        if (nvx < -TP_SPEED_CAP)
            nvx = -TP_SPEED_CAP;
        if (nvy > TP_SPEED_CAP)
            nvy = TP_SPEED_CAP;
        if (nvy < -TP_SPEED_CAP)
            nvy = -TP_SPEED_CAP;

        /* guarantee ejection off the exit surface */
        if (out_or == 0 && nvy > -TP_EJECT_MIN)
            nvy = -TP_EJECT_MIN;
        else if (out_or == 2 && nvy < TP_EJECT_MIN)
            nvy = TP_EJECT_MIN;
        else if (out_or == 1 && nvx < TP_EJECT_MIN)
            nvx = TP_EJECT_MIN;
        else if (out_or == 3 && nvx > -TP_EJECT_MIN)
            nvx = -TP_EJECT_MIN;

        *mx = (s32)((s16)(ecx - (w >> 1))) << 8;
        *my = (s32)((s16)(ecy - (h >> 1))) << 8;
        *mvx = nvx;
        *mvy = nvy;
        *cool = TP_COOLDOWN;
#ifdef TEST_BUILD
        dbg_tpcount++;
#endif
        return 1;
    }
    return 0;
}
