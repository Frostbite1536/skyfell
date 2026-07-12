/* vblank.c — the full vblank-queue engine (INV-HW-001), ported from sibling
 * prophet's hardened drain (its D-061 rewrite) with skyfell adjustments:
 * BG1/BG2 vertical scroll shadows (a platformer scrolls both axes) and
 * budget constants re-derived on THIS ROM (see the measurement note below).
 */
#include "src/core/vblank.h"

#ifdef TEST_BUILD
extern u16 dbg_vbl_last;  /* dbg.asm +26 */
extern u16 dbg_vbl_max;   /* dbg.asm +28 */
extern u16 dbg_flags;     /* dbg.asm +30, bit0 = queue overflow */
extern u16 dbg_vbl_defer; /* dbg.asm +32 — cumulative deferred entries */
extern u16 dbg_vbl_v;     /* dbg.asm +42 — scanline where the drain started */
#endif

/* NMI DMA channel 6 registers (the lib's helpers use 0; the ISR's OAM
 * transfer uses 7 — 6 is ours alone). */
#define REG_DMAP6 (*(vuint8 *)0x4360)  /* mode */
#define REG_BBAD6 (*(vuint8 *)0x4361)  /* B-bus target ($18 VMDATA, $22 CGDATA) */
#define REG_A1T6 (*(vuint16 *)0x4362)  /* source address */
#define REG_A1B6 (*(vuint8 *)0x4364)   /* source bank */
#define REG_DAS6 (*(vuint16 *)0x4365)  /* byte count */
#define REG_MDMAEN (*(vuint8 *)0x420B)
#define DMA6_ENABLE 0x40

/* Vertical-counter latch registers (for the drain's per-NMI byte budget).
 * Reading $2137 latches the H/V counters into $213C/$213D; $213D returns the
 * low 8 bits of OPVCT then bit0 of the high byte on the next read (the read
 * flip-flop that STAT78's read resets). */
#define REG_SLHV (*(vuint8 *)0x2137)  /* software latch H/V counter */
#define REG_OPVCT (*(vuint8 *)0x213D) /* vertical counter (2 reads: lo, hi bit0) */

/* Per-NMI byte budget arithmetic:
 * VQ_BYTES_PER_LINE: DMA moves one byte per 8 master cycles; a scanline is
 *   1364 master cycles => ~170.6 bytes/scanline. 160 leaves ~6% margin for
 *   the raw transfer portion of a kick. Hardware physics — carries verbatim
 *   from prophet.
 * VQ_ENTRY_TAX: the per-entry cost that is NOT transfer bytes — the tcc816
 *   loop body plus the ~7 channel-6 setup stores per entry, in DMA-byte
 *   equivalents. Prophet CALIBRATED 640 from its ROM: drain starting at
 *   v=230 (music eating ISR time), the physical cliff was 8 tiny entries per
 *   vblank; 640 defers at 7 with one entry of hardware margin. Skyfell's
 *   drain runs the same tcc-compiled loop shape on the same lib ISR, so the
 *   per-entry CPU cost is the same physics. MEASURED ON THIS ROM
 *   (2026-07-11, dbg_vbl_v probe under live queue load): drain start
 *   v=230 min=max — the lib ISR's pre-callback work (OAM DMA, DP
 *   relocation) dominates, matching prophet's music-era number. Budget
 *   31*160=4960; 4960/(4+640)=7.7 → defers at the 8th tiny entry, one
 *   below prophet's measured hardware cliff. Re-derive from dbg_vbl_v
 *   (+42) if the ISR ever grows (e.g. when SNESMOD lands, Phase 3.5). */
#define VQ_BYTES_PER_LINE 160
#define VQ_ENTRY_TAX 640

/* Payload staging lives at a FIXED address (vqdata.asm, $7E:F000) so the
 * DMA source registers are constants + offset. CPU-copying words in tcc code
 * (~100 cycles/word) slid prophet's first drain past the end of vblank; DMA
 * finishes the whole frame in a few hundred cycles. The 4KB/frame ceiling
 * (INV-HW-002) is the DMA budget. */
#define VQ_DATA_ADDR 0xF000
#define VQ_DATA_BANK 0x7E
#define VQ_MAX_WORDS 512 /* == the ".vqdata" section size */
#define VQ_MAX_ENT 24

#define VQ_KIND_SEQ 0
#define VQ_KIND_COL 1
#define VQ_KIND_CGRAM 2
#define VQ_KIND_M7SEQ 3 /* Mode 7 map bytes, +1 tile   ($2118, VMAIN $00) */
#define VQ_KIND_M7COL 4 /* Mode 7 map bytes, +128 tiles ($2118, VMAIN $02) */

extern u16 vq_data[]; /* vqdata.asm */

/* Parallel arrays (SoA), indexed by a u8 — tcc816 addresses a u16[] with a
 * SHIFT, never the software multiply helper a struct array forces (prophet's
 * frame-budget landmine). DMA register values are precomputed at PUSH time
 * so the drain is pure register stores. */
static u16 vq_addr[VQ_MAX_ENT]; /* VRAM word addr / CGRAM color index    */
static u16 vq_src[VQ_MAX_ENT];  /* A1T6 value: VQ_DATA_ADDR + (off << 1) */
static u16 vq_len[VQ_MAX_ENT];  /* DAS6 value: words << 1 (bytes)        */
static u8 vq_kind[VQ_MAX_ENT];
static u8 vq_n;
static u8 vq_head; /* drain resume index — a partial NMI defers the tail here */
static u16 vq_off;

#ifdef TEST_BUILD
static u16 vq_budget_ovr; /* if nonzero, the drain uses this instead of the
                            measured budget — deterministic squeeze testing */
#endif

static u16 sh_bg1x, sh_bg1y, sh_bg2x, sh_bg2y;
static u8 sh_m7on;
static s16 sh_m7[8]; /* a b c d x y hofs vofs */

void vq_init(void)
{
    vq_n = 0;
    vq_head = 0;
    vq_off = 0;
#ifdef TEST_BUILD
    vq_budget_ovr = 0;
#endif
    /* Scroll shadows are non-const globals (WRAM garbage until written) and
     * the drain writes them every frame — reset them here (repo reset-fn
     * convention; every scene sets scroll on entry anyway). */
    sh_bg1x = 0;
    sh_bg1y = 0;
    sh_bg2x = 0;
    sh_bg2y = 0;
    sh_m7on = 0;
}

void vq_set_m7_on(u8 on)
{
    sh_m7on = on;
}

void vq_set_m7(s16 a, s16 b, s16 c, s16 d, s16 x, s16 y, s16 hofs, s16 vofs)
{
    sh_m7[0] = a;
    sh_m7[1] = b;
    sh_m7[2] = c;
    sh_m7[3] = d;
    sh_m7[4] = x;
    sh_m7[5] = y;
    sh_m7[6] = hofs;
    sh_m7[7] = vofs;
}

void vq_set_scroll(u16 bg1x, u16 bg1y)
{
    sh_bg1x = bg1x;
    sh_bg1y = bg1y;
}

void vq_set_scroll_bg2(u16 bg2x, u16 bg2y)
{
    sh_bg2x = bg2x;
    sh_bg2y = bg2y;
}

static u8 vq_push(u8 kind, u16 addr, const u16 *src, u16 words)
{
    u16 i;
    u16 o;
    if (vq_n >= VQ_MAX_ENT || (u16)(vq_off + words) > VQ_MAX_WORDS)
    {
#ifdef TEST_BUILD
        dbg_flags |= 1;
#endif
        return 0;
    }
    o = vq_off;
    for (i = 0; i < words; i++)
        vq_data[o + i] = src[i];
    /* Precompute the DMA source address (A1T6) and byte count (DAS6) here so
     * the drain never multiplies: vq_src = payload base + byte offset. */
    vq_addr[vq_n] = addr;
    vq_src[vq_n] = (u16)(VQ_DATA_ADDR + (o << 1));
    vq_len[vq_n] = (u16)(words << 1);
    vq_kind[vq_n] = kind;
    vq_off = o + words;
    vq_n++;
    return 1;
}

u8 vq_push_vram_seq(u16 vaddr, const u16 *src, u16 words)
{
    return vq_push(VQ_KIND_SEQ, vaddr, src, words);
}

u16 vq_stage(u16 words, u8 entries)
{
    u16 o;
    if ((u8)(vq_n + entries) > VQ_MAX_ENT ||
        (u16)(vq_off + words) > VQ_MAX_WORDS)
    {
#ifdef TEST_BUILD
        dbg_flags |= 1;
#endif
        return 0xFFFF;
    }
    o = vq_off;
    vq_off = (u16)(vq_off + words);
    return o;
}

static void vq_commit(u8 kind, u16 vaddr, u16 off, u16 words)
{
    vq_addr[vq_n] = vaddr;
    vq_src[vq_n] = (u16)(VQ_DATA_ADDR + (off << 1));
    vq_len[vq_n] = (u16)(words << 1);
    vq_kind[vq_n] = kind;
    vq_n++;
}

void vq_commit_seq(u16 vaddr, u16 off, u16 words)
{
    vq_commit(VQ_KIND_SEQ, vaddr, off, words);
}

void vq_commit_col(u16 vaddr, u16 off, u16 words)
{
    vq_commit(VQ_KIND_COL, vaddr, off, words);
}

u8 vq_push_vram_col(u16 vaddr, const u16 *src, u8 words)
{
    return vq_push(VQ_KIND_COL, vaddr, src, words);
}

u8 vq_push_cgram(u8 color, const u16 *src, u8 count)
{
    return vq_push(VQ_KIND_CGRAM, color, src, count);
}

/* Mode 7 map bytes (the chamber's portal overlay): tile-index address,
 * byte payload packed into the staging words. col=1 walks +128 (a vertical
 * strip in one DMA). */
u8 vq_push_m7map(u16 tile_addr, const u8 *src, u8 bytes, u8 col)
{
    u16 words = (u16)((bytes + 1) >> 1);
    u8 i;
    u8 *dst;
    u16 o;
    if (vq_n >= VQ_MAX_ENT || (u16)(vq_off + words) > VQ_MAX_WORDS)
    {
#ifdef TEST_BUILD
        dbg_flags |= 1;
#endif
        return 0;
    }
    o = vq_off;
    dst = (u8 *)&vq_data[o];
    for (i = 0; i < bytes; i++)
        dst[i] = src[i];
    vq_addr[vq_n] = tile_addr;
    vq_src[vq_n] = (u16)(VQ_DATA_ADDR + (o << 1));
    vq_len[vq_n] = bytes;
    vq_kind[vq_n] = col ? VQ_KIND_M7COL : VQ_KIND_M7SEQ;
    vq_off = (u16)(o + words);
    vq_n++;
    return 1;
}

/* NMI drain. Called by the pvsneslib VBlank ISR with a relocated direct page
 * (interrupt.h), so tcc's imaginary registers in the interrupted main thread
 * are safe. Runs on EVERY NMI; only touches the queue's PPU targets when the
 * main loop is parked in WaitForVBlank (vblank_flag set). */
static void vq_nmi(void)
{
    u8 e;
    u16 a;
    u16 v;
    u16 budget;
    u16 cost;
    u16 drained;

    if (!vblank_flag)
        return; /* lag frame: main may be mid-push — don't drain */

    /* scroll shadows first: pairs this frame's streamed tiles with their
     * scroll (BGnVOFS displays +1 line; camera code owns that convention) */
    if (sh_m7on)
    {
        /* Mode 7 chamber: matrix + pivot + scroll, 16 stores, atomic */
        *(vuint8 *)0x211B = (u8)sh_m7[0];
        *(vuint8 *)0x211B = (u8)(sh_m7[0] >> 8);
        *(vuint8 *)0x211C = (u8)sh_m7[1];
        *(vuint8 *)0x211C = (u8)(sh_m7[1] >> 8);
        *(vuint8 *)0x211D = (u8)sh_m7[2];
        *(vuint8 *)0x211D = (u8)(sh_m7[2] >> 8);
        *(vuint8 *)0x211E = (u8)sh_m7[3];
        *(vuint8 *)0x211E = (u8)(sh_m7[3] >> 8);
        *(vuint8 *)0x211F = (u8)sh_m7[4];
        *(vuint8 *)0x211F = (u8)(sh_m7[4] >> 8);
        *(vuint8 *)0x2120 = (u8)sh_m7[5];
        *(vuint8 *)0x2120 = (u8)(sh_m7[5] >> 8);
        REG_BG1HOFS = (u8)sh_m7[6];
        REG_BG1HOFS = (u8)(sh_m7[6] >> 8);
        REG_BG1VOFS = (u8)sh_m7[7];
        REG_BG1VOFS = (u8)(sh_m7[7] >> 8);
    }
    else
    {
        REG_BG1HOFS = (u8)sh_bg1x;
        REG_BG1HOFS = (u8)(sh_bg1x >> 8);
        REG_BG1VOFS = (u8)sh_bg1y;
        REG_BG1VOFS = (u8)(sh_bg1y >> 8);
        REG_BG2HOFS = (u8)sh_bg2x;
        REG_BG2HOFS = (u8)(sh_bg2x >> 8);
        REG_BG2VOFS = (u8)sh_bg2y;
        REG_BG2VOFS = (u8)(sh_bg2y >> 8);
    }

    if (vq_head >= vq_n)
    {
        /* nothing queued (or the previous NMI fully drained): compact & bail */
        vq_head = 0;
        vq_n = 0;
        vq_off = 0;
#ifdef TEST_BUILD
        dbg_vbl_last = 0;
#endif
        return;
    }

    /* Measure how much vblank is left and turn it into a DMA-byte budget.
     * Read $213F first to reset the OPVCT read flip-flop, latch H/V via
     * $2137, then read the vertical counter low byte + high bit. Each read
     * feeds the next expression so tcc cannot elide it. */
    v = REG_STAT78;            /* reset the OPVCT read flip-flop            */
    v = REG_SLHV;              /* latch H/V counters ($213C/$213D)          */
    v = REG_OPVCT;             /* vertical counter low 8 bits               */
    v |= (REG_OPVCT & 1) << 8; /* high bit -> bit8 (0..261 fits in 9 bits)  */

#ifdef TEST_BUILD
    dbg_vbl_v = v; /* drain-start scanline — the budget-calibration probe */
#endif

    /* NTSC, no overscan: NMI fires at the start of line 225; vblank spans
     * lines 225..261. Budget = remaining scanlines * bytes-per-line. Using
     * 261 (not the true last line 262) as the terminus is a built-in
     * one-line safety margin — the drain always finishes a scanline early. */
    if (v >= 225 && v <= 261)
        budget = (u16)(261 - v) * VQ_BYTES_PER_LINE;
    else
        budget = 0;

#ifdef TEST_BUILD
    if (vq_budget_ovr)
        budget = vq_budget_ovr; /* deterministic squeeze (mailbox verb 3) */
#endif

    /* One DMA per entry, from vq_head. Each entry costs its byte count PLUS
     * a fixed CPU-setup tax; when the next entry won't fit the remaining
     * budget we DEFER the tail (leave head + records in place) — the next
     * NMI resumes. */
    REG_A1B6 = VQ_DATA_BANK;
    drained = 0;
    for (e = vq_head; e < vq_n; e++)
    {
        cost = vq_len[e] + VQ_ENTRY_TAX;
        if (cost > budget)
            break; /* defer this entry and everything after it */
        budget -= cost;

        a = vq_addr[e];
        if (vq_kind[e] == VQ_KIND_CGRAM)
        {
            REG_CGADD = (u8)a;
            REG_DMAP6 = 0x00; /* one register, write once */
            REG_BBAD6 = 0x22; /* CGDATA */
        }
        else if (vq_kind[e] >= VQ_KIND_M7SEQ)
        {
            /* Mode 7 map bytes: low VRAM byte only, inc-on-low; $02 = +128
             * (a vertical strip of tiles in one DMA) */
            REG_VMAIN = (vq_kind[e] == VQ_KIND_M7COL) ? 0x02 : 0x00;
            REG_VMADDLH = a;
            REG_DMAP6 = 0x00; /* one register */
            REG_BBAD6 = 0x18; /* VMDATAL */
        }
        else
        {
            /* VMAIN: $80 = +1 word (runs), $81 = +32 words — the hardware
             * walks a 32-wide tilemap column natively */
            REG_VMAIN = (vq_kind[e] == VQ_KIND_COL) ? 0x81 : 0x80;
            REG_VMADDLH = a;
            REG_DMAP6 = 0x01; /* two registers, write once ($2118/$2119) */
            REG_BBAD6 = 0x18; /* VMDATA */
        }
        REG_A1T6 = vq_src[e];
        REG_DAS6 = vq_len[e];
        REG_MDMAEN = DMA6_ENABLE;
        drained += vq_len[e];
    }

#ifdef TEST_BUILD
    dbg_vbl_last = drained;
    if (drained > dbg_vbl_max)
        dbg_vbl_max = drained;
#endif

    if (e >= vq_n)
    {
        /* everything drained — compact the queue */
        vq_head = 0;
        vq_n = 0;
        vq_off = 0;
    }
    else
    {
        /* partial: carry the tail to the next NMI. The payload data and the
         * entry records stay put; vq_off is NOT reset, so pushes keep landing
         * past the undrained payload. Sustained overstuffing therefore never
         * compacts off/n and eventually trips the vq_push overflow flag —
         * that is the intended INV-HW-002 back-pressure signal, by design. */
        vq_head = e;
#ifdef TEST_BUILD
        dbg_vbl_defer += (u16)(vq_n - e);
#endif
    }
}

void vq_install(void)
{
    vq_init();
    /* WRIO ($4201) bit7 must be high for the $2137 software counter latch the
     * drain's budget read uses. It is the reset default; writing it here
     * makes the drain self-sufficient on real hardware. */
    *(vuint8 *)0x4201 = 0xFF;
    nmiSet(vq_nmi);
}

#ifdef TEST_BUILD
u16 vq_scanline(void)
{
    u16 v;
    v = REG_STAT78; /* reset the OPVCT read flip-flop */
    v = REG_SLHV;   /* latch */
    v = REG_OPVCT;
    v |= (REG_OPVCT & 1) << 8;
    return v;
}

/* Force the drain's per-NMI byte budget to `bytes` (0 = back to the measured
 * value). Lets test_vblank squeeze the drain to exactly one entry per NMI so
 * the defer/carry-over path is deterministic. The squeeze value it uses (700)
 * = one 2-word entry (4 bytes) + VQ_ENTRY_TAX (640) = 644 <= 700 < 1288 (two
 * entries never fit); keep them in sync if the tax changes. */
void vq_test_budget(u16 bytes)
{
    vq_budget_ovr = bytes;
}
#endif

/* ---- force-blank only ---- */

void fb_vram_write(u16 vaddr, const u16 *src, u16 words)
{
    u16 i;
    REG_VMAIN = 0x80;
    REG_VMADDLH = vaddr;
    for (i = 0; i < words; i++)
        REG_VMDATALH = src[i];
}

void fb_vram_fill(u16 vaddr, u16 value, u16 words)
{
    u16 i;
    REG_VMAIN = 0x80;
    REG_VMADDLH = vaddr;
    for (i = 0; i < words; i++)
        REG_VMDATALH = value;
}
