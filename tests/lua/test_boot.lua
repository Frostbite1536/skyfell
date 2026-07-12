-- test_boot — Phase 0 gate: the ROM boots, the TEST debug block is alive at
-- $7E:FF00 (magic 0x51FE, D-010), the frame counter advances 1/frame
-- (INV-HW-003), and the backdrop color cycle proves the vblank-queue stub is
-- draining CGRAM writes (INV-HW-001).

H.maskInput() -- host keyboard must never reach a deterministic run

H.run(function()
    H.waitAlive(10)
    H.waitFrames(30) -- half a second of steady-state run time

    H.assertEq(H.dbgU16(0), 0x51FE, "dbg magic @ +0", 10)

    local f1 = H.dbgU16(2)
    H.waitFrames(10)
    local f2 = H.dbgU16(2)
    local d = f2 - f1
    if d < 8 or d > 12 then
        H.fail(string.format("frame counter delta %d over 10 frames (want ~10, +/-2)", d), 11)
    end

    -- vblank-queue stub proof: the backdrop (CGRAM entry 0) is cycling, and
    -- the queue never overflowed (+22, INV-HW-002). main.c's green channel
    -- steps every 4 frames, so 20 frames apart can never alias.
    H.assertEq(H.dbgU8(22), 0, "vq overflow flag @ +22", 15)
    if emu.memType.snesCgRam ~= nil then
        local c1 = H.cgramWord(0)
        H.waitFrames(20)
        local c2 = H.cgramWord(0)
        if c1 == c2 then
            H.fail(string.format("backdrop not cycling: CGRAM[0] stuck at 0x%04X", c1), 14)
        end
    end

    H.snap("boot")
    H.pass()
end)
