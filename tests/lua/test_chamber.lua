-- test_chamber — Phase 3 spike gate: the Mode 7 chamber loads (checksum
-- valid), Wren lands on the arena floor, gravity re-targets to each wall
-- with the eased rotation, and physics holds in every gravity frame.
-- Screenshots at each 90 degrees (ROADMAP test_gravity_cycle grows from
-- this). Stop codes 10..11, 14..19.

H.maskInput()
local function read32(off)
    return H.dbgU16(off) + H.dbgU16(off + 2) * 65536
end
local function verb(c, a0, a1)
    H.writeU16(DBG_BASE + 38, a0)
    H.writeU16(DBG_BASE + 40, a1)
    H.writeU16(DBG_BASE + 36, c)
    H.waitUntil(function() return H.dbgU16(36) == 0 end, 150, "verb ack", 11)
end

H.run(function()
    H.waitAlive(10)

    -- enter the chamber via the warp mailbox
    H.warp(1, 14)
    H.waitUntil(function() return H.dbgU8(18) == 1 end, 120, "chamber room id", 14)
    H.assertEq(H.dbgU8(23), 1, "chamber checksum", 15)

    -- Wren falls to the arena floor: box top 736-30=706 (py32 = 180736)
    H.waitUntil(function() return read32(8) == 706 * 256 end, 180,
                "land on the floor", 16)
    H.assertEq(H.dbgU8(16), 0, "gravity down", 16)
    H.waitFrames(20)
    H.snap("chamber_g0")

    -- gravity -> RIGHT WALL (3): rotation eases, then Wren falls +x and
    -- lands on the right wall's inner face (x = 736, sideways box 30 wide
    -- -> box left settles at 706)
    verb(9, 3, 0)
    H.waitUntil(function()
        return H.dbgU8(17) ~= 5 and math.floor(read32(4) / 256) == 706
    end, 300, "land on the right wall", 17)
    H.waitFrames(10)
    H.snap("chamber_g3")

    -- gravity -> CEILING (2): falls -y onto the top wall's inner face
    verb(9, 2, 0)
    H.waitUntil(function()
        return H.dbgU8(17) ~= 5 and math.floor(read32(8) / 256) == 288
    end, 300, "land on the ceiling", 18)
    H.waitFrames(10)
    H.snap("chamber_g2")

    -- gravity -> LEFT WALL (1), then back DOWN — the full cycle
    verb(9, 1, 0)
    H.waitUntil(function()
        return H.dbgU8(17) ~= 5 and math.floor(read32(4) / 256) == 288
    end, 300, "land on the left wall", 19)
    H.snap("chamber_g1")
    verb(9, 0, 0)
    H.waitUntil(function()
        return H.dbgU8(17) ~= 5 and math.floor(read32(8) / 256) == 706
    end, 300, "back on the floor", 19)
    H.snap("chamber_g0_again")

    H.assertEq(H.dbgU8(22), 0, "no queue overflow", 19)
    H.assertEq(H.dbgU16(34), 0, "no lag frames", 19)
    H.pass()
end)
