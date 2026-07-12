#include "src/game/entity.h"

#include "src/game/player.h"
#include "src/game/portal.h"
#include "src/game/room.h"
#include "src/game/tuning.h"

#ifdef TEST_BUILD
extern u8 dbg_entn;    /* dbg.asm +21 */
extern u16 dbg_e0x;    /* +50..+56: watched entity mirror */
extern u16 dbg_e0y;
extern u16 dbg_e0vx;
extern u16 dbg_e0vy;
extern u16 dbg_ewatch; /* +58: low8 slot, bit15 live */
#endif

/* SoA pool (tcc816: parallel arrays, never a struct array) */
static u8 e_type[ENT_MAX];
static s32 e_x[ENT_MAX]; /* box top-left, 16.8 */
static s32 e_y[ENT_MAX];
static s16 e_vx[ENT_MAX]; /* 8.8 */
static s16 e_vy[ENT_MAX];
static u8 e_st[ENT_MAX];  /* crate: 2=carried; sentry: bit7 dead, bit0 face */
static u8 e_tmr[ENT_MAX]; /* sentry fire clock */
static u8 e_cool[ENT_MAX]; /* per-object teleport cooldown */
static u8 e_slp[ENT_MAX]; /* crate at rest: full physics skipped (the frame
                             was measured within ~2k cycles of the lag cliff
                             on seam frames — resting crates must be free) */
static u8 r_hid[ENT_MAX]; /* render: slot already hidden, skip the store */
static u8 carried; /* crate slot in hand, 0xFF none */

/* compact crate list — the player clamps run per frame and must not scan
 * 16 slots; rebuilt once per ent_update */
static u8 crate_slots[ENT_MAX];
static u8 crate_cnt;

/* box size per type (w==h for all entities) */
static u8 ent_w(u8 t)
{
    if (t == ET_CRATE)
        return CRATE_W;
    if (t == ET_SENTRY)
        return 16;
    return SHOT_BOX;
}

static void rebuild_crate_list(void)
{
    u8 i;
    crate_cnt = 0;
    for (i = 0; i < ENT_MAX; i++)
        if (e_type[i] == ET_CRATE && e_st[i] != 2)
            crate_slots[crate_cnt++] = i;
}

void ent_wake_all(void)
{
    u8 i;
    for (i = 0; i < ENT_MAX; i++)
        e_slp[i] = 0;
}

void ent_clear_all(void)
{
    u8 i;
    for (i = 0; i < ENT_MAX; i++)
    {
        e_type[i] = ET_NONE;
        e_x[i] = 0;
        e_y[i] = 0;
        e_vx[i] = 0;
        e_vy[i] = 0;
        e_st[i] = 0;
        e_tmr[i] = 0;
        e_cool[i] = 0;
        e_slp[i] = 0;
        r_hid[i] = 0;
    }
    carried = 0xFF;
    crate_cnt = 0;
#ifdef TEST_BUILD
    dbg_entn = 0;
#endif
}

u8 ent_spawn(u8 type, u16 x, u16 y)
{
    u8 i;
    for (i = 0; i < ENT_MAX; i++)
    {
        if (e_type[i] == ET_NONE)
        {
            e_type[i] = type;
            e_x[i] = (s32)x << 8;
            e_y[i] = (s32)y << 8;
            e_vx[i] = 0;
            e_vy[i] = 0;
            e_st[i] = 0;
            e_tmr[i] = 0;
            e_cool[i] = 0;
            e_slp[i] = 0;
            if (type == ET_CRATE)
                rebuild_crate_list();
            return i;
        }
    }
    return 0xFF;
}

void ent_set_face(u8 slot, u8 face)
{
    e_st[slot] = (u8)((e_st[slot] & 0xFE) | (face & 1));
}

void ent_set_vel(u8 slot, s16 vx, s16 vy)
{
    e_vx[slot] = vx;
    e_vy[slot] = vy;
}

u8 ent_count(void)
{
    u8 i, n = 0;
    for (i = 0; i < ENT_MAX; i++)
        if (e_type[i] != ET_NONE)
            n++;
    return n;
}

/* room 0's authored spawns (a real .o16 pipeline arrives when rooms
 * multiply — one room doesn't earn it yet) */
void ent_room_init(u8 room)
{
    u8 s;
    ent_clear_all();
    (void)room;
    ent_spawn(ET_CRATE, 320, 434);          /* on the floor mid-left */
    s = ent_spawn(ET_SENTRY, 896, 432);     /* right shaft, between the
                                               brass tower and pillar */
    ent_set_face(s, 1);                     /* fires left, at the tower */
}

u8 ent_occupies_tile(u16 tx, u16 ty)
{
    u8 i;
    s16 x0 = (s16)(tx << 3);
    s16 y0 = (s16)(ty << 3);
    s16 ex, ey;
    u8 w;
    for (i = 0; i < ENT_MAX; i++)
    {
        if (e_type[i] == ET_NONE || e_st[i] == 2)
            continue;
        w = ent_w(e_type[i]);
        ex = (s16)(e_x[i] >> 8);
        ey = (s16)(e_y[i] >> 8);
        if (ex < (s16)(x0 + 8) && (s16)(ex + w) > x0 &&
            ey < (s16)(y0 + 8) && (s16)(ey + w) > y0)
            return 1;
    }
    return 0;
}

/* --- room collision for an entity-sized box (same sweep shape as the
 * player's, generalized on w/h) --- */
static u8 solid_at(s16 tx, s16 ty)
{
    return (u8)(ATTR_COL(room_attr((u16)tx, (u16)ty)) != COL_EMPTY);
}

static u8 solid_row_w(s16 x, s16 ty, u8 w)
{
    s16 t0 = x >> 3;
    s16 t1 = (s16)(x + w - 1) >> 3;
    s16 t;
    for (t = t0; t <= t1; t++)
        if (solid_at(t, ty))
            return 1;
    return 0;
}

static u8 solid_col_h(s16 tx, s16 y, u8 h)
{
    s16 t0 = y >> 3;
    s16 t1 = (s16)(y + h - 1) >> 3;
    s16 t;
    for (t = t0; t <= t1; t++)
        if (solid_at(tx, t))
            return 1;
    return 0;
}

/* crate-vs-crate: clamp mover i against every OTHER live grounded crate */
static void crate_clamp_x(u8 self, s16 *x, s16 *vx)
{
    u8 i;
    s16 cx, cy;
    s16 sy = (s16)(e_y[self] >> 8);
    for (i = 0; i < ENT_MAX; i++)
    {
        if (i == self || e_type[i] != ET_CRATE || e_st[i] == 2)
            continue;
        cx = (s16)(e_x[i] >> 8);
        cy = (s16)(e_y[i] >> 8);
        if (sy >= (s16)(cy + CRATE_H) || (s16)(sy + CRATE_H) <= cy)
            continue;
        if (*x < (s16)(cx + CRATE_W) && (s16)(*x + CRATE_W) > cx)
        {
            if (*vx > 0)
                *x = (s16)(cx - CRATE_W);
            else if (*vx < 0)
                *x = (s16)(cx + CRATE_W);
            *vx = 0;
        }
    }
}

/* one physics frame for a crate */
static void crate_frame(u8 i)
{
    s16 x, y, t;
    u8 landed = 0;

    if (e_st[i] == 2)
    {
        /* carried: ride above Wren's head, no physics */
        e_x[i] = (s32)((s16)(player_px() + ((PB_W - CRATE_W) >> 1))) << 8;
        e_y[i] = (s32)((s16)(player_py() - CRATE_H - 2)) << 8;
        e_vx[i] = 0;
        e_vy[i] = 0;
        return;
    }
    if (e_slp[i])
        return; /* at rest — woken by push/grab/portal changes */

    /* friction + gravity */
    if (e_vx[i] > 0)
    {
        e_vx[i] -= CRATE_FRIC;
        if (e_vx[i] < 0)
            e_vx[i] = 0;
    }
    else if (e_vx[i] < 0)
    {
        e_vx[i] += CRATE_FRIC;
        if (e_vx[i] > 0)
            e_vx[i] = 0;
    }
    e_vy[i] += P_GRAV;
    if (e_vy[i] > P_TERM_VY)
        e_vy[i] = P_TERM_VY;

    /* X sweep vs room + other crates */
    e_x[i] += e_vx[i];
    x = (s16)(e_x[i] >> 8);
    y = (s16)(e_y[i] >> 8);
    if (e_vx[i] > 0)
    {
        t = (s16)((s16)(x + CRATE_W - 1) >> 3);
        if (solid_col_h(t, y, CRATE_H))
        {
            x = (s16)((t << 3) - CRATE_W);
            e_vx[i] = 0;
        }
    }
    else if (e_vx[i] < 0)
    {
        t = (s16)(x >> 3);
        if (solid_col_h(t, y, CRATE_H))
        {
            x = (s16)((t + 1) << 3);
            e_vx[i] = 0;
        }
    }
    crate_clamp_x(i, &x, &e_vx[i]);
    e_x[i] = (s32)x << 8;

    /* Y sweep vs room (crates don't stack in v1 — pushing resolves) */
    e_y[i] += e_vy[i];
    x = (s16)(e_x[i] >> 8);
    y = (s16)(e_y[i] >> 8);
    if (e_vy[i] > 0)
    {
        t = (s16)((s16)(y + CRATE_H - 1) >> 3);
        if (solid_row_w(x, t, CRATE_W))
        {
            y = (s16)((t << 3) - CRATE_H);
            e_vy[i] = 0;
            landed = 1;
        }
    }
    else if (e_vy[i] < 0)
    {
        t = (s16)(y >> 3);
        if (solid_row_w(x, t, CRATE_W))
        {
            y = (s16)((t + 1) << 3);
            e_vy[i] = 0;
        }
    }
    e_y[i] = (s32)y << 8;

    portal_check(&e_x[i], &e_y[i], &e_vx[i], &e_vy[i], &e_cool[i],
                 CRATE_W, CRATE_H);

    /* landed with no drift -> sleep until pushed/grabbed/portal change */
    if (landed && e_vx[i] == 0 && e_cool[i] == 0)
        e_slp[i] = 1;
}

/* a moving shot: portal transit first, then wall/target checks.
 * Returns 1 if the shot despawned. */
static u8 shot_frame(u8 i)
{
    s16 ox, oy, x, y;
    s16 otx, oty, tx, ty;
    u16 a;
    u8 j;
    u8 t = e_type[i];

    ox = (s16)((s16)(e_x[i] >> 8) + (SHOT_BOX >> 1));
    oy = (s16)((s16)(e_y[i] >> 8) + (SHOT_BOX >> 1));
    e_x[i] += e_vx[i];
    e_y[i] += e_vy[i];

    if (portal_check(&e_x[i], &e_y[i], &e_vx[i], &e_vy[i], &e_cool[i],
                     SHOT_BOX, SHOT_BOX))
        return 0; /* rode the rift */

    x = (s16)((s16)(e_x[i] >> 8) + (SHOT_BOX >> 1));
    y = (s16)((s16)(e_y[i] >> 8) + (SHOT_BOX >> 1));
    tx = (s16)(x >> 3);
    ty = (s16)(y >> 3);

    /* sentry shots kill their own kind (the reflect solve) + fizzle on Wren */
    if (t == ET_SSHOT)
    {
        for (j = 0; j < ENT_MAX; j++)
        {
            if (e_type[j] == ET_SENTRY && !(e_st[j] & 0x80))
            {
                s16 sx = (s16)(e_x[j] >> 8);
                s16 sy = (s16)(e_y[j] >> 8);
                if (x >= sx && x < (s16)(sx + 16) && y >= sy && y < (s16)(sy + 16))
                {
                    e_st[j] |= 0x80; /* the sentry dies to its own shot */
                    e_type[i] = ET_NONE;
                    return 1;
                }
            }
        }
        if (x >= (s16)(player_px() - 1) && x < (s16)(player_px() + PB_W + 1) &&
            y >= player_py() && y < (s16)(player_py() + PB_H))
        {
            e_type[i] = ET_NONE; /* no damage in Phase 2 (ROADMAP) */
            return 1;
        }
    }

    /* solid contact (overlay-aware: portal cells read empty, so shots fly
     * into an open portal's zone and the plane check above catches them) */
    a = room_attr((u16)tx, (u16)ty);
    if (ATTR_COL(a) != COL_EMPTY)
    {
        if (t == ET_SHOT_B || t == ET_SHOT_G)
        {
            /* which face did we cross? compare the previous center tile */
            otx = (s16)(ox >> 3);
            oty = (s16)(oy >> 3);
            if (otx != tx && !solid_at(otx, ty))
                portal_try_place((u8)(t == ET_SHOT_G), (u16)tx, (u16)ty,
                                 (u8)((e_vx[i] > 0) ? 3 : 1));
            else
                portal_try_place((u8)(t == ET_SHOT_G), (u16)tx, (u16)ty,
                                 (u8)((e_vy[i] > 0) ? 0 : 2));
        }
        e_type[i] = ET_NONE;
        return 1;
    }
    return 0;
}

static void sentry_frame(u8 i)
{
    u8 s;
    if (e_st[i] & 0x80)
        return; /* dead husk */
    e_tmr[i]++;
    if (e_tmr[i] >= SENTRY_PERIOD)
    {
        e_tmr[i] = 0;
        if (e_st[i] & 1)
        {
            s = ent_spawn(ET_SSHOT, (u16)((e_x[i] >> 8) - SHOT_BOX - 2),
                          (u16)((e_y[i] >> 8) + 6));
            if (s != 0xFF)
                e_vx[s] = -SSHOT_SPD;
        }
        else
        {
            s = ent_spawn(ET_SSHOT, (u16)((e_x[i] >> 8) + 16 + 2),
                          (u16)((e_y[i] >> 8) + 6));
            if (s != 0xFF)
                e_vx[s] = SSHOT_SPD;
        }
    }
}

void ent_update(void)
{
    u8 i;
    u8 t;
    for (i = 0; i < ENT_MAX; i++)
    {
        t = e_type[i];
        if (t == ET_NONE)
            continue;
        if (t == ET_CRATE)
            crate_frame(i);
        else if (t == ET_SENTRY)
            sentry_frame(i);
        else
            shot_frame(i);
    }
#ifdef TEST_BUILD
    dbg_entn = ent_count();
    i = (u8)dbg_ewatch;
    if (i < ENT_MAX)
    {
        dbg_e0x = (u16)(e_x[i] >> 8);
        dbg_e0y = (u16)(e_y[i] >> 8);
        dbg_e0vx = (u16)e_vx[i];
        dbg_e0vy = (u16)e_vy[i];
        dbg_ewatch = (u16)(i | ((u16)e_st[i] << 8) |
                           ((e_type[i] != ET_NONE) ? 0x8000 : 0));
    }
#endif
}

/* --- player coupling --- */

u8 ent_clamp_x(s32 *mpx, s16 *mvx, s16 y, u8 w, u8 h, u8 push)
{
    u8 k, i;
    u8 hit = 0;
    s16 x = (s16)(*mpx >> 8);
    s16 cx, cy;
    for (k = 0; k < crate_cnt; k++)
    {
        i = crate_slots[k];
        if (e_st[i] == 2)
            continue;
        cx = (s16)(e_x[i] >> 8);
        cy = (s16)(e_y[i] >> 8);
        if (y >= (s16)(cy + CRATE_H) || (s16)(y + h) <= cy)
            continue;
        if (x < (s16)(cx + CRATE_W) && (s16)(x + w) > cx)
        {
            if (*mvx > 0)
            {
                x = (s16)(cx - w);
                if (push)
                {
                    e_vx[i] = CRATE_PUSH;
                    e_slp[i] = 0;
                }
            }
            else if (*mvx < 0)
            {
                x = (s16)(cx + CRATE_W);
                if (push)
                {
                    e_vx[i] = -CRATE_PUSH;
                    e_slp[i] = 0;
                }
            }
            *mpx = (s32)x << 8;
            hit = 1;
        }
    }
    return hit;
}

u8 ent_clamp_y(s32 *mpy, s16 *mvy, s16 x, u8 w, u8 h)
{
    u8 k, i;
    u8 stood = 0;
    s16 y = (s16)(*mpy >> 8);
    s16 cx, cy;
    for (k = 0; k < crate_cnt; k++)
    {
        i = crate_slots[k];
        if (e_st[i] == 2)
            continue;
        cx = (s16)(e_x[i] >> 8);
        cy = (s16)(e_y[i] >> 8);
        if (x >= (s16)(cx + CRATE_W) || (s16)(x + w) <= cx)
            continue;
        if (y < (s16)(cy + CRATE_H) && (s16)(y + h) > cy)
        {
            if (*mvy > 0)
            {
                y = (s16)(cy - h);
                *mvy = 0;
                stood = 1;
            }
            else if (*mvy < 0)
            {
                y = (s16)(cy + CRATE_H);
                *mvy = 0;
            }
            *mpy = (s32)y << 8;
        }
    }
    return stood;
}

void ent_grab_toggle(void)
{
    u8 i;
    s16 pcx, pcy, ccx, ccy, dx, dy;
    if (carried != 0xFF)
    {
        /* throw with Wren's facing */
        i = carried;
        e_st[i] = 0;
        e_slp[i] = 0;
        e_vx[i] = player_face() ? -THROW_VX : THROW_VX;
        e_vy[i] = THROW_VY;
        e_cool[i] = 0;
        carried = 0xFF;
        rebuild_crate_list();
        return;
    }
    pcx = (s16)(player_px() + (PB_W >> 1));
    pcy = (s16)(player_py() + (PB_H >> 1));
    for (i = 0; i < ENT_MAX; i++)
    {
        if (e_type[i] != ET_CRATE || e_st[i] == 2)
            continue;
        ccx = (s16)((s16)(e_x[i] >> 8) + (CRATE_W >> 1));
        ccy = (s16)((s16)(e_y[i] >> 8) + (CRATE_H >> 1));
        dx = (s16)(ccx - pcx);
        if (dx < 0)
            dx = (s16)(-dx);
        dy = (s16)(ccy - pcy);
        if (dy < 0)
            dy = (s16)(-dy);
        if (dx <= GRAB_RANGE && dy <= GRAB_RANGE)
        {
            e_st[i] = 2;
            e_slp[i] = 0;
            carried = i;
            rebuild_crate_list();
            return;
        }
    }
}

/* OAM sprites 2..17, one 16x16 per slot (shots draw their 6x6 orb centered
 * in the transparent 16x16 frame). Direct shadow stores (prophet's perf
 * lesson). Base tile names: crate 64, sentry 66/68, shotB 70, shotG 72,
 * sshot 74 (mkobj.py entity band). */
void ent_render(void)
{
    u8 i;
    u16 o;
    s16 sx, sy;
    u8 t;
    u8 name;
    for (i = 0; i < ENT_MAX; i++)
    {
        o = (u16)((2 + i) << 2);
        t = e_type[i];
        if (t == ET_NONE)
        {
            if (!r_hid[i])
            {
                oamMemory[o + 1] = 0xF0;
                r_hid[i] = 1;
            }
            continue;
        }
        r_hid[i] = 0;
        if (t == ET_CRATE)
        {
            name = 64;
            sx = (s16)((s16)(e_x[i] >> 8) - 1 - (s16)room_cam_x());
            sy = (s16)((s16)(e_y[i] >> 8) - 1 - (s16)room_cam_y());
        }
        else if (t == ET_SENTRY)
        {
            name = (u8)((e_st[i] & 0x80) ? 68 : 66);
            sx = (s16)((s16)(e_x[i] >> 8) - (s16)room_cam_x());
            sy = (s16)((s16)(e_y[i] >> 8) - (s16)room_cam_y());
        }
        else
        {
            name = (u8)((t == ET_SHOT_B) ? 70 : (t == ET_SHOT_G) ? 72 : 74);
            sx = (s16)((s16)(e_x[i] >> 8) - 5 - (s16)room_cam_x());
            sy = (s16)((s16)(e_y[i] >> 8) - 5 - (s16)room_cam_y());
        }
        if (sx < -15 || sx > 255 || sy < -15 || sy > 222)
        {
            oamMemory[o + 1] = 0xF0; /* off-screen: hide (X8 stays 0) */
            continue;
        }
        oamMemory[o] = (u8)sx;
        oamMemory[o + 1] = (u8)sy;
        oamMemory[o + 2] = name;
        oamMemory[o + 3] = (u8)(0x20 | ((t == ET_SENTRY && (e_st[i] & 1)) ? 0x40 : 0));
    }
}
