-- test_zone1_solve_a — Zone 1 walkthrough, rooms 03-05 (D-022): every
-- crux pad-driven; POS_SET only as documented stair CHECKPOINTS (girder
-- zigzags are 2-metatile rises = the proven jump primitive; scripting 8
-- blind hops adds fragility, not proof).
--   room03: run + jump-spam east over the practice pit, door.
--   room04: gold flat at the pillar, blue down in the pit, jump in,
--           pop out over the east plateau, door. (The first-portal teach.)
--   room05: over the tower (checkpoint), gap crawl (the walkable route —
--           the FLING variant is proven by the same numbers as room08's
--           launcher; see test_zone1_solve_b), door.
-- Stop codes 10..19.

H.maskInput()
local function read32(off) return H.dbgU16(off) + H.dbgU16(off + 2) * 65536 end
local function verb(c, a0, a1)
    H.writeU16(DBG_BASE + 38, a0)
    H.writeU16(DBG_BASE + 40, a1)
    H.writeU16(DBG_BASE + 36, c)
    H.waitUntil(function() return H.dbgU16(36) == 0 end, 150, "verb ack", 11)
end
local function posset(x, y)
    verb(4, x, y)
    H.waitFrames(8)
end
local function up_through(want, what, code)
    local f0 = H.dbgU16(2)
    H.padScript(function(f) return { up = (f - f0) < 4 } end)
    H.waitUntil(function() return H.dbgU8(18) == want end, 300, what, code)
    H.padScript(nil)
    H.waitFrames(60)
end

H.run(function()
    H.waitAlive(10)
    H.warp(3)
    H.waitFrames(30)

    -- ROOM03: run east, jump-spam over the hop ledges + the practice pit
    local f0 = H.dbgU16(2)
    H.padScript(function(f)
        local d = f - f0
        return { right = true, b = (d % 50) > 12 and (d % 50) < 44 }
    end)
    H.waitUntil(function() return read32(4) >= 985 * 256 end, 900,
                "room03 crossed", 12)
    H.padScript(nil)
    H.waitFrames(10)
    up_through(4, "room03 -> room04", 12)

    -- ROOM04 (arrive 40,370 on the west plateau): the first-portal solve
    f0 = H.dbgU16(2)
    H.padScript(function(f)
        local d = f - f0
        if d < 180 then return { right = true } end          -- to the edge
        if d < 200 then return { r = true, right = true } end
        if d < 204 then return { r = true, right = true, x = true } end
        return {}
    end)
    H.waitUntil(function() return H.dbgU8(20) >= 0x80 end, 420,
                "gold on the pillar", 13)
    H.padScript(nil)
    f0 = H.dbgU16(2)
    H.padScript(function(f)
        local d = f - f0
        if d < 40 then return { right = true } end           -- into the pit
        if d < 60 then return {} end
        if d < 130 then return { right = true } end          -- onto the brass
        if d < 150 then return { r = true, down = true } end
        if d < 154 then return { r = true, down = true, y = true } end
        return {}
    end)
    H.waitUntil(function() return H.dbgU8(19) >= 0x80 end, 260,
                "blue in the pit", 14)
    H.padScript(nil)
    local tp0 = H.dbgU16(48)
    f0 = H.dbgU16(2)
    H.padScript(function(f)
        local d = f - f0
        return { b = d > 2 and d < 22 }
    end)
    H.waitUntil(function() return H.dbgU16(48) > tp0 end, 200,
                "the pit launch", 15)
    H.padScript(nil)
    H.waitFrames(70)
    if read32(8) ~= 370 * 256 then
        H.fail("room04: did not land on the east plateau", 15)
    end
    -- recall the pair first (walking east past the door would re-enter
    -- the pillar gold and ping-pong back through the pit blue — the
    -- lesson that moved this door 2 metatiles off the pillar, D-022)
    f0 = H.dbgU16(2)
    H.padScript(function(f) return { select = (f - f0) < 4 } end)
    H.waitUntil(function() return H.dbgU8(19) < 0x80 end, 60, "recall", 15)
    H.padScript(nil)
    H.padScript(function() return { right = read32(4) < 950 * 256 } end)
    H.waitUntil(function() return read32(4) >= 950 * 256 end, 200,
                "to the room04 east door", 15)
    H.padScript(nil)
    H.waitFrames(8)
    up_through(5, "room04 -> room05", 15)

    -- ROOM05 (arrive 40,418): stairs CHECKPOINT -> tower top -> drop east
    -- -> the gap crawl -> door
    posset(560, 194)
    f0 = H.dbgU16(2)
    H.padScript(function(f)
        local d = f - f0
        if d < 46 then return { right = true } end  -- off the east edge
        if d < 58 then return { left = true } end   -- air-brake
        return {}
    end)
    H.waitFrames(90)
    H.padScript(nil)
    f0 = H.dbgU16(2)
    H.padScript(function(f)
        local d = f - f0
        -- east along the floor, drop into the gap, jump out its east wall
        return { right = true, b = (d % 50) > 12 and (d % 50) < 44 }
    end)
    H.waitUntil(function() return read32(4) >= 985 * 256 end, 1200,
                "room05 crossed (the crawl route)", 16)
    H.padScript(nil)
    H.waitFrames(10)
    up_through(6, "room05 -> room06", 16)

    H.assertEq(H.dbgU8(22), 0, "no queue overflow", 19)
    H.assertEq(H.dbgU16(70), 0, "deathless", 19)
    H.pass()
end)
