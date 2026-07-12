-- test_boot — the ROM comes alive: debug magic (D-010), frame counter
-- advancing 1/frame (INV-HW-003), room 0 loaded with a VALID checksum
-- (INV-ENG-005), vblank queue healthy (INV-HW-002), map window in VRAM.
-- Stop codes 10..17 (skipping 12/13 per the harness convention).

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

    -- room 0 loaded, checksum validated by the ROM itself (room.c)
    H.assertEq(H.dbgU8(18), 0, "room id @ +18", 14)
    H.assertEq(H.dbgU8(23), 1, "room checksum status @ +23", 15)

    -- vblank queue healthy through boot + steady state
    H.assertEq(H.dbgU8(22), 0, "vq overflow flag @ +22", 16)

    -- the map window is real: world tile (0,0) is the hull corner (tile 1)
    H.assertEq(H.vramWord(0x3000), 0x0001, "map corner word @ $3000", 17)

    H.snap("boot")
    H.pass()
end)
