-- test_jump — ROADMAP Phase 1 gate: the full-height jump apex is a GOLDEN
-- number asserted exactly, forever; landing returns to the exact floor
-- contact; a short tap rises measurably less (variable height).
--
-- GOLDENS (harvested FROM THE ROM 2026-07-12, tuning.h @ the GDD-aligned
-- Phase 1 freeze: P_GRAV 0x30, P_JUMP_VY -0x3C0, P_TERM_VY 0x400). A change
-- requires a DECISIONS.md entry (CLAUDE.md).
local APEX_PY32 = 97888   -- min py during a 30-frame-held jump from y=418
                          -- (rise 35.6px ~= 2.2 metatiles, the GDD feel)
local LAND_PY32 = 107008  -- 418<<8: exact floor contact after landing
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
local function jump_apex(hold_frames)
    local f0 = H.dbgU16(2)
    H.padScript(function(f) return { b = (f - f0) < hold_frames } end)
    local apex = read32(8)
    for _ = 1, 90 do
        coroutine.yield()
        local py = read32(8)
        if py < apex then apex = py end
    end
    H.padScript(nil)
    return apex
end

H.run(function()
    H.waitAlive(10)

    -- (a) full jump: golden apex, exact landing, idle after
    posset(88, 418)
    local apex = jump_apex(30)
    H.assertEq(apex, APEX_PY32, "full jump apex (golden)", 14)
    H.assertEq(read32(8), LAND_PY32, "exact floor contact", 15)
    H.assertEq(H.dbgU8(17), 0, "fsm idle after landing", 16)

    -- (b) variable height: a 4-frame tap must rise at least 8px less
    posset(88, 418)
    local short = jump_apex(4)
    if short < APEX_PY32 + 8 * 256 then
        H.fail(string.format("short jump too high: apex %d vs full %d",
                             short, APEX_PY32), 17)
    end
    H.assertEq(read32(8), LAND_PY32, "short jump lands exactly too", 18)

    H.assertEq(H.dbgU16(34), 0, "no lag frames", 19)
    H.snap("jump")
    H.pass()
end)
