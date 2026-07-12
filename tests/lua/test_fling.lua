-- test_fling — ROADMAP Phase 2 gate: momentum conservation through a
-- portal pair. Wren falls into a floor portal at terminal velocity
-- (0x400 = 4 px/f, exactly, by the terminal clamp) and must exit the
-- west-facing wall portal at EXACTLY -0x400 horizontal (the LUT maps
-- (0,+v) -> (-v,0) for in=up,out=left... in the mirrored-tangent frame:
-- conserved |v|, INV-ENG-003) and sail the gap.
-- Geometry (assets/maps/room01.txt): blue on the brass floor pad (tiles
-- 92-103 row 56, hit 96 -> strip 94-99, center px 776); gold on the brass
-- tower's west face (tile col 104, hit row 40 -> strip rows 38-43,
-- plane x 832, center y 328).
-- Stop codes 10..11, 14..19.

H.maskInput()
local function read32(off)
    return H.dbgU16(off) + H.dbgU16(off + 2) * 65536
end
local function posset(x, y)
    H.writeU16(DBG_BASE + 38, x)
    H.writeU16(DBG_BASE + 40, y)
    H.writeU16(DBG_BASE + 36, 4)
    H.waitUntil(function() return H.dbgU16(36) == 0 end, 150,
                "posset(" .. x .. "," .. y .. ") ack", 11)
    H.waitFrames(8)
end
local function verb(c, a0, a1)
    H.writeU16(DBG_BASE + 38, a0)
    H.writeU16(DBG_BASE + 40, a1)
    H.writeU16(DBG_BASE + 36, c)
    H.waitUntil(function() return H.dbgU16(36) == 0 end, 60, "verb ack", 11)
end

H.run(function()
    H.waitAlive(10)

    -- park Wren away from the pad AND outside the sentry's activation
    -- radius (no shot may be alive during the placement frames)
    posset(560, 418)
    verb(6, 0 + 0 * 256, 96 + 56 * 256)  -- blue, orient up, floor pad
    H.assertEq(H.dbgU8(19), 0x80, "blue placed on the floor pad", 14)
    H.waitFrames(4)
    -- the portal RENDERS: strip start tile (94,56) = blue horizontal cap
    -- (tile 29), slot (94&63=30 screen0, 56&31=24) -> 0x3000+(24<<5)+30
    H.assertEq(H.vramWord(0x331E), 29, "blue portal cap tile in VRAM", 14)
    verb(6, 1 + 3 * 256, 104 + 40 * 256) -- gold, orient left, tower west face
    H.assertEq(H.dbgU8(20), 0x83, "gold placed on the tower face", 14)
    H.waitFrames(4)
    H.snap("fling_portals")

    -- drop Wren onto the blue portal from high above: entry vy = terminal.
    -- x=790: inside the blue strip (752-800) but CLEAR of the row-18 girder
    -- (ends at px 783) that a more central drop would land on.
    posset(790, 200)
    H.waitUntil(function() return H.dbgU16(48) >= 1 end, 240, "teleport", 15)

    -- momentum conserved EXACTLY: entry (0, +0x400) -> exit (-0x400, 0)
    H.assertEq(H.dbgU16(12), 0x10000 - 0x400, "exit vx = -entry |v|", 16)

    -- and the fling is real: Wren sails left past the pit's far side
    H.waitUntil(function()
        local x = read32(4)
        return x > 0 and x < 700 * 256
    end, 240, "sailing the gap", 17)
    H.snap("fling_flight")
    H.assertEq(H.dbgU8(22), 0, "no queue overflow", 18)
    H.assertEq(H.dbgU16(34), 0, "no lag frames", 19)
    H.pass()
end)
