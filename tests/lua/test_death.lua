-- test_death — Phase 3.5 unit C (D-018): a sentry shot kills Wren; death
-- rides the door-fade path back to the CURRENT room's entry as a FULL
-- reset (portals recalled, entities respawned — INV-GAME-001's room-reset
-- guarantee). Wren parks between the brass tower and the sentry (inside
-- its activation radius, in its line of fire). Stop codes 10..11, 14..19.

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
    H.assertEq(H.dbgU16(70), 0, "no deaths at boot", 14)

    -- park in the shaft between tower and sentry; the next volley connects
    verb(4, 868, 418)
    H.waitFrames(8)
    H.waitUntil(function() return H.dbgU16(70) == 1 end, 400, "the hit", 15)

    -- respawn at the room entry after the fade + reload
    H.waitUntil(function() return read32(4) == 88 * 256 end, 300,
                "respawn at the hall entry", 16)
    H.assertEq(H.dbgU8(18), 0, "still the hall", 16)
    H.waitFrames(60) -- load + fade-in settle
    H.assertEq(H.dbgU8(21), 2, "entities respawned (full reset)", 17)
    H.assertEq(H.dbgU16(70), 1, "exactly one death", 17)
    H.snap("respawn")

    H.assertEq(H.dbgU8(22), 0, "no queue overflow", 19)
    H.assertEq(H.dbgU16(34), 0, "no lag frames", 19)
    H.pass()
end)
