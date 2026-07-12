-- test_walk — ROADMAP Phase 1 gate: scripted right-walk reaches the GOLDEN
-- x exactly (determinism means exact, not tolerance); the wall stops
-- movement exactly at the boundary.
--
-- GOLDENS (harvested FROM THE ROM 2026-07-12, tuning.h @ the GDD-aligned
-- Phase 1 freeze: P_WALK_ACC 0x28, P_WALK_MAX 0x180, P_FRICTION 0x20).
-- Changing them requires a DECISIONS.md entry (CLAUDE.md).
-- The runway is the long gantry (row 20, cols 32-44): the only ~200px
-- obstacle-free stretch after the rung-ladder respace.
local WALK60_PX32 = 154568 -- 60 held frames + friction glide from x=512
local WALL_PX32 = 16 * 256 -- box pinned flush on the 2-tile left wall
-- Stop codes 10..11, 14..19.

H.maskInput()
local function read32(off)
    return H.dbgU16(off) + H.dbgU16(off + 2) * 65536
end
local function posset(x, y)
    H.writeU16(DBG_BASE + 38, x)
    H.writeU16(DBG_BASE + 40, y)
    H.writeU16(DBG_BASE + 36, 4)
    H.waitUntil(function() return H.dbgU16(36) == 0 end, 60, "posset ack", 11)
    H.waitFrames(8)
end

H.run(function()
    H.waitAlive(10)

    -- (a) walk: exactly 60 held frames of right, then friction to a stop
    posset(512, 290) -- standing on the long gantry
    local f0 = H.dbgU16(2)
    H.padScript(function(f) return { right = (f - f0) < 60 } end)
    H.waitUntil(function()
        return (H.dbgU16(2) - f0) > 60 and H.dbgU16(12) == 0
    end, 200, "walk stop", 14)
    H.padScript(nil)
    H.assertEq(read32(4), WALK60_PX32, "walk golden x", 15)
    H.assertEq(H.dbgU8(17), 0, "fsm idle after stop", 16)

    -- (b) wall: hold left into the wall; pin EXACTLY at x=16 and stay there
    posset(88, 418)
    H.padScript(function(f) return { left = true } end)
    H.waitUntil(function() return read32(4) == WALL_PX32 end, 300, "wall pin", 17)
    H.waitFrames(20) -- keep pushing: no penetration, no jitter
    H.assertEq(read32(4), WALL_PX32, "still pinned while pushing", 18)
    H.assertEq(H.dbgU16(12), 0, "vx zero against the wall", 18)
    H.padScript(nil)

    H.assertEq(H.dbgU16(34), 0, "no lag frames", 19)
    H.snap("walk")
    H.pass()
end)
