-- test_replay — INV-ENG-002, established here and GREEN FOREVER: the same
-- input script from the same start state produces BIT-IDENTICAL player
-- state at every checkpoint. This is the fence every later phase (portals,
-- gravity, entities, audio) must not break.
-- Stop codes 10..11, 14..16.

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

-- the choreography: run, jump onto the step, back off, hop, settle
local function script(r)
    return {
        right = r < 40 or (r >= 44 and r < 70),
        b = (r >= 44 and r < 70) or (r >= 124 and r < 140),
        left = r >= 80 and r < 120,
    }
end

local CHECKPOINTS = { 60, 120, 180 }

local function run_once()
    posset(88, 418)
    local f0 = H.dbgU16(2)
    H.padScript(function(f) return script(f - f0) end)
    local snaps = {}
    local ci = 1
    while ci <= #CHECKPOINTS do
        coroutine.yield()
        local r = H.dbgU16(2) - f0
        if r >= CHECKPOINTS[ci] then
            snaps[ci] = string.format("px=%d py=%d vx=%d vy=%d fsm=%d",
                                      read32(4), read32(8), H.dbgU16(12),
                                      H.dbgU16(14), H.dbgU8(17))
            ci = ci + 1
        end
    end
    H.padScript(nil)
    return snaps
end

H.run(function()
    H.waitAlive(10)
    local a = run_once()
    local b = run_once()
    for i = 1, #CHECKPOINTS do
        if a[i] ~= b[i] then
            H.fail(string.format("replay diverged @ checkpoint %d:\nA: %s\nB: %s",
                                 CHECKPOINTS[i], a[i], b[i]), 14)
        end
    end
    H.assertEq(H.dbgU16(34), 0, "no lag frames", 15)
    H.assertEq(H.dbgU8(22), 0, "no queue overflow", 16)
    H.snap("replay")
    H.pass()
end)
