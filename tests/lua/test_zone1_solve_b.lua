-- test_zone1_solve_b — Zone 1 walkthrough, rooms 06-09 (D-022): every
-- crux pad-driven; POS_SET only as documented stair/walkway CHECKPOINTS.
--   room06: grab the crate, carry it to the 48px plateau face, throw,
--           crate-hop up (the 34px hop inside the 35.6px apex).
--   room07: gold down-right from the tower top into the pillar, drop in
--           behind the EAST-facing sentry, blue on the tower face,
--           retreat up the pillar, its own shot comes home (D-022).
--   room08: synthesis — gap crawl, gold up-left into the tower face,
--           blue on the pad, jump-in fling (gap-assisted landing),
--           then the crate hop onto the exit plateau.
--   room09: the switchback CHECKPOINT, UP at the gate -> the chamber.
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
local function walk_to(x, code, what)
    H.padScript(function() return { right = read32(4) < x * 256 } end)
    H.waitUntil(function() return read32(4) >= x * 256 end, 400, what, code)
    H.padScript(nil)
    H.waitFrames(8)
end

H.run(function()
    H.waitAlive(10)
    H.warp(6)
    H.waitFrames(30)

    -- ROOM06: walk to the crate (pins against it), grab, carry, throw
    walk_to(400, 12, "to the crate")
    H.waitFrames(20) -- pinned/pushing a touch is fine — still in range
    local f0 = H.dbgU16(2)
    H.padScript(function(f)
        local d = f - f0
        if d < 4 then return { a = true } end
        return {}
    end)
    H.waitFrames(10)
    H.padScript(nil)
    walk_to(608, 12, "carry to the face")
    f0 = H.dbgU16(2)
    H.padScript(function(f)
        local d = f - f0
        if d > 6 and d < 10 then return { a = true } end
        return {}
    end)
    H.waitFrames(70) -- the thrown crate settles at the face base
    H.padScript(nil)
    -- the hops: onto the crate, then the 34px hop onto the plateau
    f0 = H.dbgU16(2)
    H.padScript(function(f)
        local d = f - f0
        if d < 20 then return { b = d > 2, right = true } end
        if d < 50 then return { right = d < 26 } end
        if d < 80 then return { b = d > 54, right = true } end
        if d < 120 then return { right = true } end
        return {}
    end)
    H.waitUntil(function() return read32(8) == 370 * 256 end, 200,
                "room06 plateau topped", 12)
    H.padScript(nil)
    walk_to(980, 12, "to the room06 east door")
    up_through(7, "room06 -> room07", 12)

    -- ROOM07 (arrive 40,418): the blind-spot reflect
    verb(7, 0, 0)    -- watch slot 0 = the sentry (the +58 death mirror)
    posset(650, 258) -- CHECKPOINT: stairs + walkway (proven 2-row rises)
    f0 = H.dbgU16(2)
    H.padScript(function(f)
        local d = f - f0
        if d < 16 then return { r = true, down = true, right = true } end
        if d < 20 then return { r = true, down = true, right = true, x = true } end
        return {}
    end)
    H.waitUntil(function() return H.dbgU8(20) >= 0x80 end, 150,
                "gold on the pillar", 13)
    H.padScript(nil)
    f0 = H.dbgU16(2)
    H.padScript(function(f)
        local d = f - f0
        if d < 18 then return { right = true } end
        if d < 28 then return { left = true } end
        return {}
    end)
    H.waitFrames(70) -- drop into the shaft, behind the sentry
    H.padScript(nil)
    f0 = H.dbgU16(2)
    H.padScript(function(f)
        local d = f - f0
        if d < 16 then return { r = true, left = true } end
        if d < 20 then return { r = true, left = true, y = true } end
        return {}
    end)
    H.waitUntil(function() return H.dbgU8(19) >= 0x80 end, 120,
                "blue on the tower", 13)
    H.padScript(nil)
    H.waitUntil(function() return H.dbgU8(21) >= 2 end, 200, "a volley", 14)
    f0 = H.dbgU16(2)
    H.padScript(function(f)
        local d = f - f0
        if d < 60 then return { right = true } end
        if d < 84 then return { right = d > 70, b = d > 62 and d < 78 } end
        if d < 120 then return { right = true, b = d > 96 and d < 112 } end
        return {}
    end)
    H.waitFrames(140)
    H.padScript(nil)
    H.waitUntil(function() return H.dbgU16(58) >= 0x8000 end, 600,
                "the sentry dies to its own shot", 14)
    walk_to(990, 14, "to the room07 east door")
    up_through(8, "room07 -> room08", 14)

    -- ROOM08 (arrive 40,418): synthesis. Tower CHECKPOINT, drop east,
    -- gap crawl, gold up-left, back west, blue, jump-in fling, crates.
    verb(7, 0, 0) -- watch the crate
    posset(460, 194)
    f0 = H.dbgU16(2)
    H.padScript(function(f)
        local d = f - f0
        if d < 46 then return { right = true } end
        if d < 58 then return { left = true } end
        return {}
    end)
    H.waitFrames(90) -- onto the pad area (~x 533)
    H.padScript(nil)
    f0 = H.dbgU16(2)
    H.padScript(function(f) -- east over the gap (crawl: in, jump out)
        local d = f - f0
        return { right = read32(4) < 708 * 256,
                 b = (d % 50) > 12 and (d % 50) < 44 }
    end)
    H.waitUntil(function()
        return read32(4) >= 708 * 256 and read32(8) == 418 * 256
    end, 900, "room08 gap crawled east", 15)
    H.padScript(nil)
    H.waitFrames(8)
    f0 = H.dbgU16(2)
    H.padScript(function(f)
        local d = f - f0
        if d < 20 then return { r = true, up = true, left = true } end
        if d < 24 then return { r = true, up = true, left = true, x = true } end
        return {}
    end)
    H.waitUntil(function() return H.dbgU8(20) >= 0x80 end, 200,
                "gold on the room08 tower face", 15)
    H.padScript(nil)
    f0 = H.dbgU16(2)
    H.padScript(function(f) -- back west over the gap
        local d = f - f0
        return { left = read32(4) > 548 * 256,
                 b = (d % 50) > 12 and (d % 50) < 44 }
    end)
    H.waitUntil(function()
        return read32(4) <= 548 * 256 and read32(8) == 418 * 256
    end, 900, "back west to the pad", 16)
    H.padScript(nil)
    H.waitFrames(8)
    f0 = H.dbgU16(2)
    H.padScript(function(f)
        local d = f - f0
        if d < 16 then return { r = true, down = true } end
        if d < 20 then return { r = true, down = true, y = true } end
        return {}
    end)
    H.waitUntil(function() return H.dbgU8(19) >= 0x80 end, 120,
                "blue on the room08 pad", 16)
    H.padScript(nil)
    -- the jump-in fling (gap-assisted: lands at the gap's east end)
    local tp0 = H.dbgU16(48)
    f0 = H.dbgU16(2)
    H.padScript(function(f)
        local d = f - f0
        return { b = d > 2 and d < 30 }
    end)
    H.waitUntil(function() return H.dbgU16(48) > tp0 end, 200,
                "the room08 fling", 17)
    H.padScript(nil)
    H.waitFrames(80)
    f0 = H.dbgU16(2)
    H.padScript(function(f) -- out of the gap; stop SHORT of the crate
        local d = f - f0    -- (jump-spam past 700 vaults clean over it)
        return { right = read32(4) < 700 * 256,
                 b = (d % 50) > 12 and (d % 50) < 44 }
    end)
    H.waitUntil(function()
        return read32(4) >= 698 * 256 and read32(8) == 418 * 256
    end, 600, "gap exited east", 17)
    H.padScript(nil)
    -- the fling lands well EAST of the crate (~810): walk back LEFT and
    -- pin flush against its east side (the grab is side-agnostic)
    H.padScript(function() return { left = read32(4) > 742 * 256 } end)
    H.waitUntil(function() return read32(4) <= 742 * 256 end, 300,
                "back to the crate", 17)
    H.padScript(nil)
    H.waitFrames(4)
    f0 = H.dbgU16(2)
    H.padScript(function(f) return { left = (f - f0) < 20 } end)
    H.waitFrames(30)
    H.padScript(nil)
    f0 = H.dbgU16(2)
    H.padScript(function(f)
        local d = f - f0
        if d < 4 then return { a = true } end
        return {}
    end)
    H.waitFrames(10)
    H.padScript(nil)
    walk_to(832, 18, "carry to the room08 face")
    f0 = H.dbgU16(2)
    H.padScript(function(f)
        local d = f - f0
        if d > 6 and d < 10 then return { a = true } end
        return {}
    end)
    H.waitFrames(70)
    H.padScript(nil)
    H.snap("r8_prehop")
    f0 = H.dbgU16(2)
    H.padScript(function(f)
        local d = f - f0
        if d < 20 then return { b = d > 2, right = true } end
        if d < 50 then return { right = d < 26 } end
        if d < 80 then return { b = d > 54, right = true } end
        if d < 120 then return { right = true } end
        return {}
    end)
    H.waitUntil(function() return read32(8) == 370 * 256 end, 200,
                "room08 plateau topped", 18)
    H.padScript(nil)
    walk_to(980, 18, "to the room08 east door")
    up_through(9, "room08 -> room09", 18)

    -- ROOM09: the switchback CHECKPOINT, then the gate
    posset(505, 194)
    up_through(1, "the chamber gate", 19)
    H.assertEq(H.dbgU8(16), 0, "chamber gravity down", 19)

    H.assertEq(H.dbgU8(22), 0, "no queue overflow", 19)
    H.pass()
end)
