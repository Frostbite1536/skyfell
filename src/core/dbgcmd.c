#include "src/core/dbgcmd.h"
#include "src/core/rng.h"
#include "src/core/vblank.h"

#ifdef TEST_BUILD
extern u16 dbg_cmd;  /* dbg.asm +36 */
extern u16 dbg_arg0; /* dbg.asm +38 */
extern u16 dbg_arg1; /* dbg.asm +40 */
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
    dbg_cmd = 0; /* ack */
#endif
}
