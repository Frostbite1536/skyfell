-- test_vblank — the drain engine's contract (ported from prophet's D-061):
--   (a) a 12-entry live burst lands WHOLE in VRAM (throughput)
--   (b) under a squeezed budget the tail DEFERS across NMIs, never drops
--   (c) true overstuffing trips the INV-HW-002 overflow flag
-- Stop codes 10..11, 14..19 (skipping 12/13 per the harness convention).
-- VRAM scratch words 0x7C00+ are test-reserved (tests/README.md).

H.maskInput()
H.run(function()
    H.waitAlive(10)

    -- (a) throughput: 12 entries = 48 bytes, one healthy NMI's work
    local function stress(n, base)
        H.writeU16(DBG_BASE + 38, n)    -- arg0
        H.writeU16(DBG_BASE + 40, base) -- arg1
        H.writeU16(DBG_BASE + 36, 2)    -- cmd = VQ_STRESS
        H.waitUntil(function() return H.dbgU16(36) == 0 end, 60, "stress ack", 11)
    end
    stress(12, 0xA100)
    H.waitFrames(4)
    for i = 0, 11 do
        H.assertEq(H.vramWord(0x7C00 + 2 * i), 0xA100 + 2 * i,
                   "burst word lo " .. i, 14)
        H.assertEq(H.vramWord(0x7C00 + 2 * i + 1), 0xA100 + 2 * i + 1,
                   "burst word hi " .. i, 14)
    end
    H.assertEq(H.dbgU16(30), 0, "flags clean after burst", 15)

    -- (b) squeeze: budget 700 = exactly one 2-word entry per NMI (vblank.c
    -- keeps 700 in sync with VQ_ENTRY_TAX). 8 entries must still all land,
    -- deferred across ~8 NMIs, with the defer counter moving and NO overflow.
    local defer0 = H.dbgU16(32)
    H.writeU16(DBG_BASE + 38, 700)
    H.writeU16(DBG_BASE + 36, 3) -- cmd = VQ_BUDGET
    H.waitUntil(function() return H.dbgU16(36) == 0 end, 60, "budget ack", 11)
    stress(8, 0xB200)
    H.waitFrames(14) -- 8 entries at 1/NMI + slack
    for i = 0, 7 do
        H.assertEq(H.vramWord(0x7C00 + 2 * i), 0xB200 + 2 * i,
                   "deferred word " .. i, 16)
    end
    if H.dbgU16(32) <= defer0 then
        H.fail("defer counter never moved under the squeeze", 17)
    end
    H.assertEq(H.dbgU16(30), 0, "no overflow from a mere defer", 18)

    -- (c) overstuffing: still squeezed, stuff 24 then 24 again a few frames
    -- later — the queue can't compact fast enough and vq_push must refuse
    -- with the sticky flag (INV-HW-002 back-pressure).
    stress(24, 0xC300)
    H.waitFrames(2)
    stress(24, 0xD400)
    H.waitUntil(function() return H.dbgU16(30) % 2 == 1 end, 30,
                "overflow flag after overstuff", 19)

    -- restore the measured budget so the tail drains before the ROM idles on
    H.writeU16(DBG_BASE + 38, 0)
    H.writeU16(DBG_BASE + 36, 3)
    H.waitUntil(function() return H.dbgU16(36) == 0 end, 60, "budget restore ack", 11)
    H.waitFrames(6)

    H.snap("vblank")
    H.pass()
end)
