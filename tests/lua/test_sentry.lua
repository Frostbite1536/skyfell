-- test_sentry — ROADMAP Phase 2 gate: the sentry's own shot, routed through
-- a portal pair, kills it (the reflect solve). Geometry: the sentry (pool
-- slot 1, room spawn at x=896 facing left) fires 2px/f shots at the brass
-- tower; gold on the tower's EAST face (tile col 107, strip rows 50-55)
-- swallows them; blue on the brass pillar's WEST face (tile col 124, strip
-- rows 50-55) ejects them moving LEFT at the same height — straight into
-- the sentry's back.
-- Stop codes 10..11, 14..19.

H.maskInput()
local function posset(x, y)
    H.writeU16(DBG_BASE + 38, x)
    H.writeU16(DBG_BASE + 40, y)
    H.writeU16(DBG_BASE + 36, 4)
    H.waitUntil(function() return H.dbgU16(36) == 0 end, 150, "posset ack", 11)
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
    -- set the trap while Wren is far away (the sentry only fires with him
    -- inside its activation radius)
    posset(88, 418)
    verb(7, 1, 0)   -- WATCH the sentry (slot 1); +58 high byte = its e_st
    H.waitFrames(2)
    local st = math.floor(H.dbgU16(58) / 256)
    if st >= 128 then
        H.fail("sentry already dead? e_st=" .. st, 14)
    end

    verb(6, 1 + 1 * 256, 107 + 52 * 256) -- gold, orient right, tower east
    H.assertEq(H.dbgU8(20), 0x81, "gold on the tower east face", 15)
    verb(6, 0 + 3 * 256, 124 + 52 * 256) -- blue, orient left, pillar west
    H.assertEq(H.dbgU8(19), 0x83, "blue on the pillar west face", 15)
    H.snap("sentry_setup")

    -- now step into range (left of the whole portal circuit — the routed
    -- shot never reaches Wren); the warp also re-zeroes the lag counter,
    -- so the final lag==0 assert measures THE KILL PHASE
    posset(700, 418)

    local tp0 = H.dbgU16(48)
    -- the sentry fires every 96 frames; the shot needs ~60 more to route
    H.waitUntil(function()
        return math.floor(H.dbgU16(58) / 256) >= 128
    end, 600, "sentry dies to its own shot", 16)

    if H.dbgU16(48) <= tp0 then
        H.fail("no teleport happened — the kill wasn't routed", 17)
    end
    H.waitFrames(8)
    H.snap("sentry_dead")
    H.assertEq(H.dbgU16(34), 0, "no lag frames", 19)
    H.pass()
end)
