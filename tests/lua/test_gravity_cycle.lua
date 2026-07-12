-- test_gravity_cycle — ROADMAP Phase 3 gate: a scripted 4-rotation loop
-- through PORTAL TRANSITS (the real mechanic: gravity = -(exit outward
-- normal)). Each hop: place a pair on brass, warp Wren beside the entry,
-- let him fall/walk through, recall, settle; screenshot per 90 degrees.
-- The WHOLE loop runs TWICE and the final state must be BIT-IDENTICAL
-- (INV-ENG-002 in the chamber).
-- Chamber facts: inner faces y=736 floor (orient 0) / x=736 right wall
-- (orient 3) / y=288 ceiling (orient 2) / x=288 left wall (orient 1);
-- brass strips: bottom/top tiles 56-71, left/right rows 56-71.
-- Stop codes 10..11, 14..19.

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
local function settle(gwant, code, what)
    H.waitUntil(function()
        return H.dbgU8(16) == gwant and H.dbgU8(17) == 0
    end, 400, what, code)
    H.waitFrames(6)
end

-- one full 4-hop loop; snap only when snapping=true (first pass)
local function loop(snapping)
    -- hop 1: floor portal (blue, orient 0, row 92) -> right wall (gold,
    -- orient 3, col 91). Wren drops onto the floor portal.
    verb(4, 512, 706)
    H.waitFrames(8)
    verb(6, 0 + 0 * 256, 64 + 92 * 256)
    verb(6, 1 + 3 * 256, 92 + 60 * 256)
    verb(4, 500, 660) -- above the floor portal strip (tiles 62-67)
    H.waitUntil(function() return H.dbgU8(16) == 3 end, 300, "hop1 transit", 14)
    verb(6, 0xFFFF, 0) -- recall both (no accidental re-entry)
    settle(3, 14, "stand on the right wall")
    if snapping then H.snap("cycle_g3") end

    -- hop 2: right wall (blue, orient 3) -> ceiling (gold, orient 2, row 35)
    verb(6, 0 + 3 * 256, 92 + 60 * 256)
    verb(6, 1 + 2 * 256, 64 + 35 * 256)
    verb(4, 690, 464) -- against the right wall, above its portal strip
    H.waitUntil(function() return H.dbgU8(16) == 2 end, 300, "hop2 transit", 15)
    verb(6, 0xFFFF, 0)
    settle(2, 15, "stand on the ceiling")
    if snapping then H.snap("cycle_g2") end

    -- hop 3: ceiling (blue, orient 2) -> left wall (gold, orient 1, col 36)
    verb(6, 0 + 2 * 256, 64 + 35 * 256)
    verb(6, 1 + 1 * 256, 35 + 60 * 256)
    verb(4, 500, 300) -- on the ceiling near its portal strip
    H.waitUntil(function() return H.dbgU8(16) == 1 end, 300, "hop3 transit", 16)
    verb(6, 0xFFFF, 0)
    settle(1, 16, "stand on the left wall")
    if snapping then H.snap("cycle_g1") end

    -- hop 4: left wall (blue, orient 1) -> floor (gold, orient 0, row 92)
    verb(6, 0 + 1 * 256, 35 + 60 * 256)
    verb(6, 1 + 0 * 256, 64 + 92 * 256)
    verb(4, 300, 464) -- against the left wall, over its portal strip
    H.waitUntil(function() return H.dbgU8(16) == 0 end, 300, "hop4 transit", 17)
    verb(6, 0xFFFF, 0)
    settle(0, 17, "back on the floor")
    if snapping then H.snap("cycle_g0") end
    -- anchor for the bit-identical compare
    verb(4, 512, 706)
    H.waitFrames(20)
    return string.format("px=%d py=%d g=%d fsm=%d",
                         read32(4), read32(8), H.dbgU8(16), H.dbgU8(17))
end

H.run(function()
    H.waitAlive(10)
    H.warp(1, 14) -- enter the chamber
    H.waitUntil(function() return H.dbgU8(18) == 1 end, 120, "chamber", 14)

    local a = loop(true)
    local b = loop(false)
    if a ~= b then
        H.fail("cycle not bit-identical:\nA: " .. a .. "\nB: " .. b, 18)
    end
    H.assertEq(H.dbgU8(22), 0, "no queue overflow", 19)
    H.assertEq(H.dbgU16(34), 0, "no lag frames", 19)
    H.pass()
end)
