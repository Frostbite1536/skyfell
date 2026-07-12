#include "src/game/room.h"

#include "src/core/vblank.h"
#include "src/data/generated/rooms.h"

extern u16 room01_map[]; /* rooms.asm (generated) — direct label indexing,
                            never a stored C pointer (tcc816 ROM rule) */
extern u16 room01_att[];
extern char tiles_chr, tiles_chr_end, tiles_pal; /* data.asm */

#ifdef TEST_BUILD
extern u8 dbg_room;    /* dbg.asm +18 */
extern u8 dbg_roomck;  /* dbg.asm +23 — 1 checksum ok, 2 BAD (INV-ENG-005) */
extern u16 dbg_camx;   /* dbg.asm +44 */
extern u16 dbg_camy;   /* dbg.asm +46 */
#endif

#define VRAM_BG1_CHR 0x0000 /* words — ARCHITECTURE.md Phase 1 layout */
#define VRAM_BG1_MAP 0x3000 /* SC_64x32: screen0 $3000, screen1 $3400 */

static u8 cur_room;
static u16 rm_w, rm_h;     /* room size in 8x8 tiles */
static u16 rm_wpx, rm_hpx; /* room size in px */
static u16 cam_x, cam_y;   /* camera top-left, px, clamped to the room */
static u16 win_x, win_y;   /* valid VRAM window top-left, tiles */

/* Streaming build buffer — static, not stack: main-thread only (the NMI
 * drain never calls into room.c), fully rewritten before every use. */
static u16 strmbuf[32];

static u16 map_at(u16 tx, u16 ty)
{
    /* every room is 128 tiles wide (the D-012 authoring grid), so the row
     * stride is a shift — revisit when a second room size ever exists */
    return room01_map[(u16)(ty << 7) + tx];
}

u16 room_attr(u16 tx, u16 ty)
{
    if (tx >= rm_w || ty >= rm_h)
        return COL_SOLID; /* out of bounds = solid plain wall */
    return room01_att[map_at(tx, ty) & 0x3FF];
}

u16 room_cam_x(void) { return cam_x; }
u16 room_cam_y(void) { return cam_y; }
u16 room_w_px(void) { return rm_wpx; }
u16 room_h_px(void) { return rm_hpx; }

/* Window placement: the visible screen is up to 33x29 tiles; the 64x32
 * window leaves 31 columns / 3 rows of slack. Keep 15 columns behind the
 * camera and 1 row above it. */
static u16 win_want_x(void)
{
    u16 vx = cam_x >> 3;
    u16 d = (vx > 15) ? (u16)(vx - 15) : 0;
    u16 mx = (u16)(rm_w - 64);
    return (d > mx) ? mx : d;
}

static u16 win_want_y(void)
{
    u16 vy = cam_y >> 3;
    u16 d = (vy > 1) ? (u16)(vy - 1) : 0;
    u16 my = (u16)(rm_h - 32);
    return (d > my) ? my : d;
}

/* VRAM word address of the map column holding world column wx */
static u16 col_addr(u16 wx)
{
    u16 mx = wx & 63;
    return (u16)(VRAM_BG1_MAP + ((mx & 32) << 5) + (mx & 31));
}

/* Stream one full world column (32 window rows, slot-ordered so the +32
 * stride DMA writes rows 0..31 in one push). */
static void push_col(u16 wx)
{
    u8 my;
    u8 s = (u8)(win_y & 31);
    for (my = 0; my < 32; my++)
        strmbuf[my] = map_at(wx, (u16)(win_y + (u8)((u8)(my - s) & 31)));
    vq_push_vram_col(col_addr(wx), strmbuf, 32);
}

/* Stream one full world row (64 window columns = 2 seq pushes, one per
 * 32x32 screen). */
static void push_row(u16 wy)
{
    u8 mx;
    u8 s = (u8)(win_x & 63);
    u16 row = (u16)((wy & 31) << 5);
    for (mx = 0; mx < 32; mx++)
        strmbuf[mx] = map_at((u16)(win_x + (u8)((u8)(mx - s) & 63)), wy);
    vq_push_vram_seq((u16)(VRAM_BG1_MAP + row), strmbuf, 32);
    for (mx = 32; mx < 64; mx++)
        strmbuf[(u8)(mx - 32)] = map_at((u16)(win_x + (u8)((u8)(mx - s) & 63)), wy);
    vq_push_vram_seq((u16)(VRAM_BG1_MAP + 0x400 + row), strmbuf, 32);
}

/* Full window redraw — force-blank ONLY (fb_vram_write is not a queue
 * client; INV-HW-001). */
static void redraw_fb(void)
{
    u8 my;
    u8 mx;
    u8 sx = (u8)(win_x & 63);
    u8 sy = (u8)(win_y & 31);
    u16 wy;
    for (my = 0; my < 32; my++)
    {
        wy = (u16)(win_y + (u8)((u8)(my - sy) & 31));
        for (mx = 0; mx < 32; mx++)
            strmbuf[mx] = map_at((u16)(win_x + (u8)((u8)(mx - sx) & 63)), wy);
        fb_vram_write((u16)(VRAM_BG1_MAP + ((u16)my << 5)), strmbuf, 32);
        for (mx = 32; mx < 64; mx++)
            strmbuf[(u8)(mx - 32)] = map_at((u16)(win_x + (u8)((u8)(mx - sx) & 63)), wy);
        fb_vram_write((u16)(VRAM_BG1_MAP + 0x400 + ((u16)my << 5)), strmbuf, 32);
    }
}

static void cam_clamp(u16 *x, u16 *y)
{
    u16 mx = (u16)(rm_wpx - 256);
    u16 my = (u16)(rm_hpx - 224);
    if (*x > mx)
        *x = mx;
    if (*y > my)
        *y = my;
}

static void cam_apply(void)
{
    /* BGnVOFS displays line VOFS+1, so -1 puts world row cam_y on the top
     * scanline; the &wrap makes cam_y==0 come out right. */
    vq_set_scroll((u16)(cam_x & 511), (u16)((cam_y - 1) & 255));
#ifdef TEST_BUILD
    dbg_camx = cam_x;
    dbg_camy = cam_y;
#endif
}

/* Place camera + window and redraw everything — force-blank only. */
static void place_cam(u16 x, u16 y)
{
    cam_clamp(&x, &y);
    cam_x = x;
    cam_y = y;
    win_x = win_want_x();
    win_y = win_want_y();
    redraw_fb();
    cam_apply();
}

void room_cam_set(u16 x, u16 y)
{
    u16 wxd, wyd;
    cam_clamp(&x, &y);
    cam_x = x;
    cam_y = y;
    wxd = win_want_x();
    wyd = win_want_y();
    while (win_x < wxd)
    {
        push_col((u16)(win_x + 64)); /* column entering on the right */
        win_x++;
    }
    while (win_x > wxd)
    {
        win_x--;
        push_col(win_x); /* column entering on the left */
    }
    while (win_y < wyd)
    {
        push_row((u16)(win_y + 32)); /* row entering below */
        win_y++;
    }
    while (win_y > wyd)
    {
        win_y--;
        push_row(win_y); /* row entering above */
    }
    cam_apply();
}

void room_cam_warp(u16 x, u16 y)
{
    setScreenOff();
    place_cam(x, y);
    setScreenOn();
    /* force-blank time is not gameplay lag (prophet's convention):
     * dbg_lag==0 must mean the LIVE loop holds 60fps */
    lag_frame_counter = 0;
}

void room_load(u8 id, u16 camx, u16 camy)
{
    setScreenOff();
    cur_room = id; /* one room in Phase 1; the id keys future room tables */
    rm_w = ROOM01_W;
    rm_h = ROOM01_H;
    rm_wpx = (u16)(rm_w << 3);
    rm_hpx = (u16)(rm_h << 3);

    /* tileset chr + palette 0 (lib helper: force-blank DMA on channel 0) */
    bgInitTileSet(0, &tiles_chr, &tiles_pal, 0,
                  (u16)(&tiles_chr_end - &tiles_chr), 16 * 2,
                  BG_16COLORS, VRAM_BG1_CHR);
    bgSetMapPtr(0, VRAM_BG1_MAP, SC_64x32);

    place_cam(camx, camy);

#ifdef TEST_BUILD
    {
        /* INV-ENG-005: re-sum the ROM data against the converter's checksum */
        u16 ck = 0;
        u16 i;
        u16 n = (u16)(rm_h << 7);
        for (i = 0; i < n; i++)
            ck += room01_map[i];
        for (i = 0; i < ROOM_ATTR_PAD; i++)
            ck += room01_att[i];
        dbg_roomck = (ck == ROOM01_CKSUM) ? 1 : 2;
        dbg_room = id;
    }
#endif

    setScreenOn();
    lag_frame_counter = 0; /* force-blank load time is not gameplay lag */
}
