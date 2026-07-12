#include "src/game/portal.h"

#include "src/core/vblank.h"
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

extern u16 room_map[];   /* the LIVE room (chamram.asm, D-016) — the
                            validator reads raw map/attr directly (no
                            calls in the profiled hot path) */
extern u16 room01_att[]; /* shared tileset attrs (roomglue asserts) */
extern u8 cham_map[];  /* chamber.asm: Mode 7 world (bytes, same 128 stride) */
extern u16 cham_att[];

u8 portal_world;         /* 0 = Mode 1 room, 1 = the chamber (map source) */
u8 portal_last_exit_or;  /* outward normal of the last EXIT portal — the
                            chamber's gravity rule reads it after a transit
                            (gravity = -(exit normal), which is the same
                            encoding: up-exit => gravity down, etc.) */

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
/* combined bounding box over both strips, tile coords INCLUSIVE — the
 * second-stage inline gate: with portals PLACED, every collision query
 * paying a portal_cell call cost ~60 scanlines per ordinary frame
 * (measured); 4 compares against these kill it for queries anywhere else */
u16 portal_bx0, portal_bx1, portal_by0, portal_by1;
static u8 m7buf[STRIP]; /* chamber refresh staging — FILE scope (tcc816
                           stack/function-static array caution) */

static void rebbox(void)
{
    u8 c;
    u16 ex, ey;
    portal_bx0 = 0xFFFF; /* empty box: no tx can be >= it */
    portal_bx1 = 0;
    portal_by0 = 0xFFFF;
    portal_by1 = 0;
    for (c = 0; c < 2; c++)
    {
        if (!p_on[c])
            continue;
        ex = (p_or[c] == 0 || p_or[c] == 2) ? (u16)(p_tx[c] + STRIP - 1)
                                            : p_tx[c];
        ey = (p_or[c] == 0 || p_or[c] == 2) ? p_ty[c]
                                            : (u16)(p_ty[c] + STRIP - 1);
        if (p_tx[c] < portal_bx0)
            portal_bx0 = p_tx[c];
        if (ex > portal_bx1)
            portal_bx1 = ex;
        if (p_ty[c] < portal_by0)
            portal_by0 = p_ty[c];
        if (ey > portal_by1)
            portal_by1 = ey;
    }
}

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
    portal_last_exit_or = 0;
    rebbox();
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

/* does color c's strip cover this tile? (c must be placed) */
static u8 cell_of(u8 c, u16 tx, u16 ty)
{
    if (horiz(p_or[c]))
        return (u8)(ty == p_ty[c] && tx >= p_tx[c] &&
                    tx < (u16)(p_tx[c] + STRIP));
    return (u8)(tx == p_tx[c] && ty >= p_ty[c] &&
                ty < (u16)(p_ty[c] + STRIP));
}

u8 portal_cell(u16 tx, u16 ty)
{
    u8 c;
    if (!p_any)
        return 0;
    for (c = 0; c < 2; c++)
        if (p_on[c] && cell_of(c, tx, ty))
            return 1;
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
    if (portal_world)
    {
        /* the chamber: Mode 7 map bytes, tile 13 blue / 14 gold when on,
         * the raw world byte when cleared; VRAM word addr == tile index.
         * Plain ifs throughout — ternaries here produced a +1-stride
         * placement (tcc816 codegen caution). */
        u8 k;
        u16 idx = (u16)((p_ty[c] << 7) + p_tx[c]);
        u8 hz = horiz(p_or[c]);
        if (hz)
        {
            /* horizontal strip: contiguous, one 6-byte push */
            for (k = 0; k < STRIP; k++)
            {
                if (p_on[c])
                    m7buf[k] = (u8)(13 + c);
                else
                    m7buf[k] = cham_map[(u16)(idx + k)];
            }
            vq_push_m7map(idx, m7buf, STRIP);
        }
        else
        {
            /* vertical strip: six 1-byte pushes. The +128-stride DMA
             * (VMAIN $02) landed +1 in MesenCE regardless of source shape
             * (measured; unresolved emu-vs-codegen) — per-cell pushes are
             * event-time cheap and unambiguous. */
            for (k = 0; k < STRIP; k++)
            {
                if (p_on[c])
                    m7buf[k] = (u8)(13 + c);
                else
                    m7buf[k] = cham_map[(u16)(idx + ((u16)k << 7))];
                vq_push_m7map((u16)(idx + ((u16)k << 7)), &m7buf[k], 1);
            }
        }
        return;
    }
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
    rebbox();
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
 * read as raw brass already since attrs come from the raw map).
 *
 * ZERO per-tile calls (D-001 after profiling: the call-per-tile version
 * measured ~160 scanlines — tcc816 calls cost ~1k cycles): direct indexed
 * ROM reads with a constant front-cell offset, one other-portal rect test,
 * one live-list entity rect scan. */
static u8 strip_valid(u8 self, u16 sx, u16 sy, u8 orient, u8 self_ent)
{
    u8 k;
    u16 idx;
    u16 a;
    u8 other = (u8)(self ^ 1);
    u8 hz = horiz(orient);
    u16 x1 = hz ? (u16)(sx + STRIP - 1) : sx; /* strip end, inclusive */
    u16 y1 = hz ? sy : (u16)(sy + STRIP - 1);
    s16 foff;      /* front cell = strip cell + this map-index offset */
    u16 rx0, ry0, rx1, ry1; /* strip+front union rect */

    /* conservative bounds: both worlds keep solid borders, so a valid
     * strip never touches the edge and every +-1 neighbor stays in range
     * (room: 128x64 tiles; chamber: 128x128) */
    if (sx < 1 || sy < 1 || x1 > 126 || y1 > (u16)(portal_world ? 126 : 62))
        return 0;

    if (orient == 0)
        foff = -128;
    else if (orient == 2)
        foff = 128;
    else if (orient == 1)
        foff = 1;
    else
        foff = -1;

    idx = (u16)((sy << 7) + sx);
    for (k = 0; k < STRIP; k++)
    {
        if (portal_world)
        {
            a = cham_att[cham_map[idx]];
            if (ATTR_COL(a) == COL_EMPTY || ATTR_MAT(a) != MAT_BRASS)
                return 0;
            a = cham_att[cham_map[(u16)(idx + foff)]];
        }
        else
        {
            a = room01_att[room_map[idx] & 0x3FF];
            if (ATTR_COL(a) == COL_EMPTY || ATTR_MAT(a) != MAT_BRASS)
                return 0;
            a = room01_att[room_map[(u16)(idx + foff)] & 0x3FF];
        }
        if (ATTR_COL(a) != COL_EMPTY)
            return 0;
        idx = (u16)(idx + (hz ? 1 : 128));
    }

    /* strip+front union rect (front is one cell along the normal) */
    rx0 = sx;
    ry0 = sy;
    rx1 = x1;
    ry1 = y1;
    if (orient == 0)
        ry0--;
    else if (orient == 2)
        ry1++;
    else if (orient == 1)
        rx1++;
    else
        rx0--;

    if (p_on[other])
    {
        /* the OTHER portal's strip may not intersect our union rect.
         * (Never portal_cell here: it sees BOTH portals, so a placed
         * portal would invalidate ITSELF at teleport revalidation — the
         * bug that silently refused every transit.) */
        u16 ox1 = horiz(p_or[other]) ? (u16)(p_tx[other] + STRIP - 1)
                                     : p_tx[other];
        u16 oy1 = horiz(p_or[other]) ? p_ty[other]
                                     : (u16)(p_ty[other] + STRIP - 1);
        if (p_tx[other] <= rx1 && ox1 >= rx0 && p_ty[other] <= ry1 &&
            oy1 >= ry0)
            return 0;
    }

    if (ent_occupies_rect(rx0, ry0, rx1, ry1, self_ent))
        return 0;
    return 1;
}

static const s8 slide[5] = {0, -1, 1, -2, 2}; /* FILE scope: the prophet
                                 function-scope-static placement trap */

u8 portal_try_place(u8 color, u16 tx, u16 ty, u8 orient, u8 self_ent)
{
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
        if (strip_valid(color, sx, sy, orient, self_ent))
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
            rebbox();
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

void portal_cool(u8 *cool)
{
    if (*cool)
        (*cool)--;
}

u8 portal_check(u8 axis, s32 *mx, s32 *my, s16 *mvx, s16 *mvy, u8 *cool,
                u8 w, u8 h, u8 self_ent)
{
    u8 c, o, in_or, out_or, k;
    s16 cx, cy;
    s16 zx0, zy0;
    s16 d;
    s16 lead;
    s16 icx, icy, ocx, ocy; /* in/out portal center points (on the plane) */
    s16 nvx, nvy;
    s16 push;
    s16 ecx, ecy;

    if (*cool)
        return 0;
    /* two SEPARATE ifs — `!p_on[0] || !p_on[1]` miscompiled under tcc816
     * (a phantom transit fired with both portals OFF; the forensics probe
     * 2026-07-12 caught p_on=={0,0} inside the success path — the same
     * codegen-trap family as prophet's u8 switch) */
    if (!p_on[0])
        return 0;
    if (!p_on[1])
        return 0;

    cx = (s16)((s16)(*mx >> 8) + (w >> 1));
    cy = (s16)((s16)(*my >> 8) + (h >> 1));

    for (c = 0; c < 2; c++)
    {
        if (!p_on[c])
            return 0; /* belt: never transit a cleared portal */
        in_or = p_or[c];
        zx0 = (s16)(p_tx[c] << 3);
        zy0 = (s16)(p_ty[c] << 3);
        if (axis == 1 && horiz(in_or))
        {
            /* floor/ceiling: tangential = center x vs the 48px strip;
             * radial = the LEADING y edge inside the 8px opening */
            if (cx < zx0 || cx >= (s16)(zx0 + 48))
                continue;
            if (in_or == 0)
            {
                if (*mvy <= 0)
                    continue; /* enter a floor portal moving down */
                lead = (s16)((s16)(*my >> 8) + h);
                if (lead <= zy0 || lead > (s16)(zy0 + 8))
                    continue;
            }
            else
            {
                if (*mvy >= 0)
                    continue;
                lead = (s16)(*my >> 8);
                if (lead < zy0 || lead >= (s16)(zy0 + 8))
                    continue;
            }
        }
        else if (axis == 0 && !horiz(in_or))
        {
            if (cy < zy0 || cy >= (s16)(zy0 + 48))
                continue;
            if (in_or == 3)
            {
                if (*mvx <= 0)
                    continue; /* enter a west-facing portal moving right */
                lead = (s16)((s16)(*mx >> 8) + w);
                if (lead <= zx0 || lead > (s16)(zx0 + 8))
                    continue;
            }
            else
            {
                if (*mvx >= 0)
                    continue;
                lead = (s16)(*mx >> 8);
                if (lead < zx0 || lead >= (s16)(zx0 + 8))
                    continue;
            }
        }
        else
            continue;

        /* INV-ENG-004 teleport revalidation, EXIT side: don't eject into
         * an occupied/blocked opening. The ENTRY side is skipped by
         * reasoned refinement (INVARIANTS change log 2026-07-12): its
         * plane was just crossed, terrain is immutable, and an entity
         * behind the mover cannot endanger a transit already through —
         * while the second strip_valid cost measured transit-frame lag. */
        o = (u8)(c ^ 1);
        if (!strip_valid(o, p_tx[o], p_ty[o], p_or[o], self_ent))
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
        portal_last_exit_or = out_or;
#ifdef TEST_BUILD
        dbg_tpcount++;
#endif
        return 1;
    }
    return 0;
}
