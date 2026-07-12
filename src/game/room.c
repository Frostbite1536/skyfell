#include "src/game/room.h"

#include "src/core/vblank.h"
#include "src/data/generated/rooms.h"
#include "src/game/portal.h"

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
 * drain never calls into room.c), fully rewritten before every use. 64
 * words: a full row builds slot-ordered in one pass, pushed as two 32s.
 * (redraw_fb still uses it; live seams write vq_data directly via asm.) */
static u16 strmbuf[64];

/* seams.asm params — non-static on purpose (the asm reads them by name) */
u16 sm_src; /* BYTE offset into room01_map */
u16 sm_dst; /* in-bank $7E BYTE address of the destination */
u16 sm_cnt; /* seam_mvn: bytes / seam_coln: words */
extern void seam_mvn(void);  /* contiguous ROM->WRAM block move */
extern void seam_coln(void); /* strided column gather (128-word stride) */
#define VQ_DATA_BYTE 0xF000  /* vq_data's in-bank address (vqdata.asm) */

static u16 map_at(u16 tx, u16 ty)
{
    /* every room is 128 tiles wide (the D-012 authoring grid), so the row
     * stride is a shift — revisit when a second room size ever exists.
     * Portal overlay first — behind the inline portal_any gate (hot path:
     * seam streaming calls this per word). */
    if (portal_any)
    {
        u16 pw = portal_map_word(tx, ty);
        if (pw)
            return pw;
    }
    return room01_map[(u16)(ty << 7) + tx];
}

u16 room_attr_raw(u16 tx, u16 ty)
{
    if (tx >= rm_w || ty >= rm_h)
        return COL_SOLID; /* out of bounds = solid plain wall */
    return room01_att[room01_map[(u16)(ty << 7) + tx] & 0x3FF];
}

u16 room_attr(u16 tx, u16 ty)
{
    if (portal_any && portal_cell(tx, ty))
        return 0; /* the opening is walk-through (portal.c owns the plane) */
    return room_attr_raw(tx, ty);
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
 * stride DMA writes rows 0..31 in one push). The gather runs in ASSEMBLY
 * straight into the queue's staging area (zero copies) — profiling showed
 * the C build-then-copy version cost ~90 scanlines and tipped seam frames
 * over the 60fps cliff (D-001: asm after the profiler fingers it). The
 * slot-wrap is hoisted into two strided runs. */
static void push_col(u16 wx)
{
    u8 s = (u8)(win_y & 31);
    u16 off = vq_stage(32, 1);
    u16 i0;
    if (off == 0xFFFF)
        return; /* frame full: the overflow flag is the signal (INV-HW-002) */
    i0 = (u16)(((u16)(win_y - s) << 7) + wx);
    /* run 1: slots s..31 = window rows win_y..win_y+(31-s) */
    sm_src = (u16)((u16)(i0 + ((u16)s << 7)) << 1);
    sm_dst = (u16)(VQ_DATA_BYTE + ((u16)(off + s) << 1));
    sm_cnt = (u16)(32 - s);
    seam_coln();
    if (s)
    {
        /* run 2: slots 0..s-1 = window rows win_y+(32-s).. (wrap) */
        sm_src = (u16)((u16)(i0 + 4096) << 1);
        sm_dst = (u16)(VQ_DATA_BYTE + (off << 1));
        sm_cnt = s;
        seam_coln();
    }
    if (portal_any)
        portal_fix_col(wx, &vq_data[off], win_y);
    vq_commit_col(col_addr(wx), off, 32);
}

/* Stream one full world row (64 window columns = 2 seq pushes, one per
 * 32x32 screen). Contiguous in ROM, so the whole row is 1-2 MVN block
 * moves into the staging area. */
static void push_row(u16 wy)
{
    u8 s = (u8)(win_x & 63);
    u16 row = (u16)((wy & 31) << 5);
    u16 off = vq_stage(64, 2);
    u16 i0;
    if (off == 0xFFFF)
        return;
    i0 = (u16)((wy << 7) + win_x - s);
    /* run 1: slots s..63 from world cols win_x.. */
    sm_src = (u16)((u16)(i0 + s) << 1);
    sm_dst = (u16)(VQ_DATA_BYTE + ((u16)(off + s) << 1));
    sm_cnt = (u16)((u16)(64 - s) << 1); /* bytes */
    seam_mvn();
    if (s)
    {
        sm_src = (u16)((u16)(i0 + 64) << 1);
        sm_dst = (u16)(VQ_DATA_BYTE + (off << 1));
        sm_cnt = (u16)((u16)s << 1);
        seam_mvn();
    }
    if (portal_any)
        portal_fix_row(wy, &vq_data[off], win_x);
    vq_commit_seq((u16)(VRAM_BG1_MAP + row), off, 32);
    vq_commit_seq((u16)(VRAM_BG1_MAP + 0x400 + row), (u16)(off + 32), 32);
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

/* re-push cells whose overlay changed (portal place/remove). One 1-word
 * push per in-window cell (<=6 per call; the drain defers as needed). */
void room_refresh_strip(u16 tx, u16 ty, u8 len, u8 vertical)
{
    u8 k;
    u16 x, y, w;
    u16 mx, my;
    for (k = 0; k < len; k++)
    {
        x = vertical ? tx : (u16)(tx + k);
        y = vertical ? (u16)(ty + k) : ty;
        if (x < win_x || x >= (u16)(win_x + 64) || y < win_y ||
            y >= (u16)(win_y + 32))
            continue;
        mx = x & 63;
        my = y & 31;
        w = map_at(x, y);
        vq_push_vram_seq((u16)(VRAM_BG1_MAP + ((mx & 32) << 5) + (my << 5) +
                               (mx & 31)),
                         &w, 1);
    }
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
