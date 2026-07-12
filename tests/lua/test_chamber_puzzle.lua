-- test_chamber_puzzle — ROADMAP Phase 3 gate: the scripted FULL SOLVE
-- reaches the exit. The puzzle (D-015, authored in chamber01.txt): the
-- crate spawns on ledge B, unreachable under gravity-down; the pressure
-- pad is in the CEILING (metatile cols 6-7 -> crate must REST against
-- y=288 with center x in 352..383, so gravity must be UP); a resting
-- crate opens the exit door in the left wall's ceiling corner (map tiles
-- tx 34-35 ty 36-39 swap to void). Solve: reorient #1 (floor -> right
-- wall) drops the crate off its ledge, grab it, reorient #2 (right wall
-- -> ceiling) carrying it, drop it on the pad, walk into the recess.
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
local function posset(x, y)
    verb(4, x, y)
    H.waitFrames(8)
end
local function settle(gwant, code, what)
    H.waitUntil(function()
        return H.dbgU8(16) == gwant and H.dbgU8(17) == 0
    end, 400, what, code)
    H.waitFrames(6)
end
-- the door's first map cell: tile (ty=36, tx=34) -> Mode 7 VRAM word
local DOOR_VRAM = 36 * 128 + 34

H.run(function()
    H.waitAlive(10)
    H.warp(1, 14)
    H.waitUntil(function() return H.dbgU8(18) == 1 end, 120, "chamber", 14)
    H.assertEq(H.dbgU8(23), 1, "chamber checksum (WRAM live map)", 14)
    local closed = H.vramWord(DOOR_VRAM) % 256
    H.assertEq(closed, 19, "door renders closed", 14)

    -- reorient #1: floor -> right wall; the crate falls off ledge B onto
    -- the wall face (it wakes on the gravity change)
    verb(6, 0 + 0 * 256, 64 + 92 * 256)  -- blue, floor brass
    verb(6, 1 + 3 * 256, 92 + 60 * 256)  -- gold, right wall brass
    posset(500, 660)
    H.waitUntil(function() return H.dbgU8(16) == 3 end, 300, "reorient 1", 15)
    verb(6, 0xFFFF, 0)
    settle(3, 15, "stand on the right wall")

    -- the crate rested against the wall below Wren; watch it, then grab
    verb(7, 1, 0) -- slot 1 = the puzzle crate (drone is slot 0)
    H.waitUntil(function()
        return H.dbgU16(50) == 722 and H.dbgU16(52) == 546
    end, 200, "crate rests on the wall face", 16)
    posset(706, 540)
    local f0 = H.dbgU16(2)
    H.padScript(function(f) return { a = (f - f0) < 4 } end)
    H.waitUntil(function()
        return math.floor(H.dbgU16(58) / 256) == 2
    end, 60, "crate grabbed (carried state)", 16)
    H.padScript(nil)

    -- reorient #2, carrying: right wall -> ceiling
    verb(6, 0 + 3 * 256, 92 + 60 * 256)  -- blue, right wall
    verb(6, 1 + 2 * 256, 64 + 35 * 256)  -- gold, ceiling
    posset(690, 464)
    H.waitUntil(function() return H.dbgU8(16) == 2 end, 300, "reorient 2", 17)
    verb(6, 0xFFFF, 0)
    settle(2, 17, "stand on the ceiling")

    -- walk zone: drop the crate on the pad (straight anti-gravity pop,
    -- D-015 — it falls back and RESTS on the ceiling over the pad cells)
    posset(363, 288)
    f0 = H.dbgU16(2)
    H.padScript(function(f) return { a = (f - f0) < 4 } end)
    H.waitUntil(function()
        return H.vramWord(DOOR_VRAM) % 256 == 0
    end, 150, "the door opens", 18)
    H.padScript(nil)
    H.snap("puzzle_door_open")

    -- walk into the recess (under gravity-up, screen-right = world -x)
    H.padScript(function() return { right = true } end)
    H.waitUntil(function() return H.dbgU16(68) == 1 end, 300, "the exit", 19)
    H.padScript(nil)
    H.snap("puzzle_exit")

    H.assertEq(H.dbgU8(22), 0, "no queue overflow", 19)
    H.assertEq(H.dbgU16(34), 0, "no lag frames", 19)
    H.pass()
end)
