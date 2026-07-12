#include "src/core/dbgcmd.h"
#include "src/core/rng.h"
#include "src/core/vblank.h"
#include "src/game/entity.h"
#include "src/game/player.h"
#include "src/game/portal.h"

#ifdef TEST_BUILD
extern u16 dbg_cmd;    /* dbg.asm +36 */
extern u16 dbg_arg0;   /* dbg.asm +38 */
extern u16 dbg_arg1;   /* dbg.asm +40 */
extern u16 dbg_ewatch; /* dbg.asm +58 */
#endif

void dbg_poll(void)
{
#ifdef TEST_BUILD
    u16 c = dbg_cmd;
    u16 n;
    u16 base;
    u16 i;
    u16 pair[2];
    if (c == 0)
        return;
    if (c == 1)
        rng_seed(((u32)dbg_arg1 << 16) | dbg_arg0);
    else if (c == 2)
    {
        /* VQ_STRESS(n = arg0 entries, base = arg1 pattern): push n live
         * 2-word VRAM entries at word addr 0x7C00 + 2*i — the test-scratch
         * words (tests/README.md): no live writer touches them, so the Lua
         * side can read exactly what the drain moved. Entry i payload =
         * {base + 2i, base + 2i + 1}. Proves throughput + defer. */
        n = dbg_arg0;
        base = dbg_arg1;
        if (n > 24)
            n = 24; /* defensive cap == VQ_MAX_ENT (vblank.c) */
        for (i = 0; i < n; i++)
        {
            pair[0] = base + (i << 1);
            pair[1] = base + (i << 1) + 1;
            vq_push_vram_seq((u16)(0x7C00 + (i << 1)), pair, 2);
        }
    }
    else if (c == 3)
        vq_test_budget(dbg_arg0); /* VQ_BUDGET(bytes, 0 = off) */
    else if (c == 4)
        player_warp(dbg_arg0, dbg_arg1); /* POS_SET(x px, y px): place the
                                            player's box top-left; zeroes
                                            motion; force-blank camera warp */
    else if (c == 5)
    {
        /* ENT_SPAWN(arg0 = type | slot-face<<8, arg1 = x_meta | y_meta<<8):
         * spawn at the metatile's top-left px; bit8 sets a sentry's face */
        u8 s = ent_spawn((u8)dbg_arg0,
                         (u16)(((u8)dbg_arg1) << 4),
                         (u16)(((u8)(dbg_arg1 >> 8)) << 4));
        if (s != 0xFF && (dbg_arg0 & 0x100))
            ent_set_face(s, 1);
    }
    else if (c == 6)
    {
        /* PORTAL_SET(arg0 = color | orient<<8, arg1 = tx | ty<<8) — goes
         * through THE validator (INV-ENG-004); 0xFFFF = recall both */
        if (dbg_arg0 == 0xFFFF)
            portal_clear();
        else
            portal_try_place((u8)(dbg_arg0 & 1), (u16)((u8)dbg_arg1),
                             (u16)((u8)(dbg_arg1 >> 8)),
                             (u8)((dbg_arg0 >> 8) & 3));
    }
    else if (c == 7)
        dbg_ewatch = dbg_arg0; /* WATCH(slot) — entity mirror at +50..+56 */
    else if (c == 8)
        ent_clear_all();
    dbg_cmd = 0; /* ack */
#endif
}
