-- test_zone1 — Phase 3.5 Zone 1 authoring (D-021): all seven Gantries
-- rooms load with valid live-map checksums, the forward door chain
-- room03 -> ... -> room09 -> the chamber connects (each door entered
-- with a real UP press in its authored rect), a backward door works,
-- and authored spawns land (room07's sentry). The full teaching-route
-- SOLVES are test_zone1_solve's (F2b). Stop codes 10..19.

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
local function posset(x, y)
    verb(4, x, y)
    H.waitFrames(8)
end
local function press_up()
    local f0 = H.dbgU16(2)
    H.padScript(function(f) return { up = (f - f0) < 4 } end)
end
local function through_door(x, y, want_room, what, code)
    posset(x, y)
    press_up()
    H.waitUntil(function() return H.dbgU8(18) == want_room end, 300, what, code)
    H.padScript(nil)
    H.waitFrames(50) -- transition tail (load + fade-in)
    H.assertEq(H.dbgU8(23), 1, what .. " checksum", code)
end

-- forward chain: {stand x, stand y, arrival room, arrival entry x}
local CHAIN = {
    { 996, 418, 4, 40 },  -- room03 east (floor) -> room04 (west plateau)
    { 980, 370, 5, 40 },  -- room04 east (plateau) -> room05
    { 996, 418, 6, 40 },  -- room05 east (floor) -> room06
    { 980, 370, 7, 40 },  -- room06 east (plateau) -> room07
    { 996, 418, 8, 40 },  -- room07 east (floor) -> room08
    { 980, 370, 9, 40 },  -- room08 east (plateau) -> room09
}

H.run(function()
    H.waitAlive(10)

    -- start at the zone's first room
    H.warp(3)
    H.waitFrames(20)
    H.assertEq(H.dbgU8(23), 1, "room03 checksum", 10)
    H.assertEq(H.dbgU8(21), 0, "room03 is empty", 10)

    for i = 1, #CHAIN do
        local c = CHAIN[i]
        through_door(c[1], c[2], c[3], "door into room0" .. c[3], 12)
        H.assertEq(read32(4), c[4] * 256, "room0" .. c[3] .. " entry x", 13)
    end

    -- room07 hosts the zone's sentry (authored @spawn)
    H.warp(7)
    H.waitFrames(20)
    H.assertEq(H.dbgU8(21), 1, "room07 spawns its sentry", 14)

    -- a backward door: room04 west -> room03 (arrive by its east door)
    H.warp(4)
    H.waitFrames(20)
    through_door(20, 370, 3, "room04 west door back to room03", 15)
    H.assertEq(read32(4), 976 * 256, "room03 east-side arrival", 15)

    -- the gate: room09 -> the chamber
    H.warp(9)
    H.waitFrames(20)
    through_door(505, 194, 1, "the chamber gate", 16)
    H.assertEq(H.dbgU8(16), 0, "chamber gravity down", 16)
    H.snap("gate_into_chamber")

    H.assertEq(H.dbgU8(22), 0, "no queue overflow", 19)
    H.assertEq(H.dbgU16(34), 0, "no lag frames", 19)
    H.pass()
end)
