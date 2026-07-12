/* RIFT: THE SKYFELL ENGINE — Phase 0 walking skeleton.
 *
 * Proves the whole pipeline end to end: builds LoROM/FastROM, boots in
 * MesenCE, draws "SKYFELL" on the pvsneslib text console, cycles the
 * backdrop color every frame THROUGH the vblank-queue stub (INV-HW-001),
 * and keeps the TEST debug block alive at $7E:FF00 (D-010) for the Lua
 * harness. No gameplay — ROADMAP Phase 0 exclusions apply.
 *
 * VRAM here is the pvsneslib hello_world example's proven layout, not the
 * game's plan (ARCHITECTURE.md's Mode 1 map arrives with Phase 1's real
 * loaders); this file exists to be replaced. */

#include <snes.h>

#include "src/core/vblank.h"

extern char tilfont, palfont; /* data.asm: Phase 0 placeholder font */

#ifdef TEST_BUILD
/* Debug block at $7E:FF00 — labels in dbg.asm (INV-TEST-001, D-010). WRAM
 * boots as garbage: every field is zeroed below BEFORE dbg_magic goes live,
 * or the Lua harness could read phantom state (prophet's boot lesson). */
extern u16 dbg_magic;  /* +0:  0x51FE when alive        */
extern u16 dbg_frame;  /* +2:  +1 per main-loop frame   */
extern s32 dbg_px;     /* +4..+15: player kinematics (Phase 1 owns) */
extern s32 dbg_py;
extern s16 dbg_vx;
extern s16 dbg_vy;
extern u8 dbg_grav;    /* +16..+21: world state (Phases 1-2 own) */
extern u8 dbg_fsm;
extern u8 dbg_room;
extern u8 dbg_pblue;
extern u8 dbg_pgold;
extern u8 dbg_entn;
extern u8 dbg_vqovf;   /* +22: vblank-queue overflow mirror (INV-HW-002) */
extern u8 dbg_roomck;  /* +23 */
extern u16 dbg_warp;   /* +24: Lua warp mailbox (Phase 1 consumes) */
#endif

int main(void)
{
    u16 t;   /* frame tick driving the color cycle */
    u8 ph;   /* cycle phase, 0..63 */
    u8 lvl;  /* triangle level, 0..31 */
    u16 c;   /* BGR555 backdrop color */

    /* Text console on the first BG, straight from the pvsneslib hello_world
     * example (screen is force-blanked until setScreenOn). */
    consoleSetTextMapPtr(0x6800);
    consoleSetTextGfxPtr(0x3000);
    consoleSetTextOffset(0x0100);
    consoleInitText(0, 16 * 2, &tilfont, &palfont);

    bgSetGfxPtr(0, 0x2000);
    bgSetMapPtr(0, 0x6800, SC_32x32);

    setMode(BG_MODE1, 0);
    bgSetDisable(1);
    bgSetDisable(2);

    consoleDrawText(12, 10, "SKYFELL");
    consoleDrawText(4, 14, "RIFT: THE SKYFELL ENGINE");
    consoleDrawText(9, 18, "PHASE 0 BOOT OK");
    /* consoleDrawText only writes the WRAM text buffer; the upload happens
     * in the NMI handler — vq_nmi chains the lib's consoleVblank for that
     * (interrupt.h:227). consoleUpdate() is NOT the answer: it DMAs to a
     * hardcoded VRAM $0800, ignoring consoleSetTextMapPtr. */

    /* vq statics are WRAM garbage until vq_init — init BEFORE the ISR can
     * see them, then hand the drain to the lib's VBlank ISR. */
    vq_init();
    nmiSet(vq_nmi);

#ifdef TEST_BUILD
    /* Zero the whole block, magic LAST: magic means "valid from here on". */
    dbg_frame = 0;
    dbg_px = 0;
    dbg_py = 0;
    dbg_vx = 0;
    dbg_vy = 0;
    dbg_grav = 0;
    dbg_fsm = 0;
    dbg_room = 0;
    dbg_pblue = 0;
    dbg_pgold = 0;
    dbg_entn = 0;
    dbg_vqovf = 0;
    dbg_roomck = 0;
    dbg_warp = 0;
    dbg_magic = 0x51FE;
#endif

    setScreenOn();

    t = 0;
    while (1)
    {
        /* Backdrop color cycle — CGRAM entry 0, pushed through the queue so
         * the write lands in vblank (INV-HW-001). Blue/red run a triangle
         * fade; green steps every 4 frames so any two nearby frames differ
         * (test_boot asserts CGRAM[0] actually changes). */
        ph = (u8)((t >> 2) & 0x3F);
        if (ph < 32)
            lvl = ph;
        else
            lvl = (u8)(63 - ph);
        c = ((u16)lvl << 10) | (((u16)(t >> 2) & 0x1F) << 5) | (u16)(31 - lvl);
        vq_push_cgram(0, c);

        WaitForVBlank();
        t++;
#ifdef TEST_BUILD
        dbg_frame = t;             /* +1 per displayed frame (INV-HW-003) */
        dbg_vqovf = vq_overflow;   /* INV-HW-002 probe */
#endif
    }
    return 0;
}
