-- test_title — Phase 3.5 (D-019/D-021): the title card (release boots into
-- it; TEST builds summon it via verb 10 so the harness never stalls) parks
-- the world, renders console text, and a FRESH START press (edge-gated —
-- a held START must not skip cards) enters Zone 1's first room through the
-- full goto_room reload. Stop codes 10..11, 14..19.

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

    -- summon the title; the world parks (frame counter keeps ticking,
    -- the player stops updating)
    verb(10, 0, 0)
    H.waitFrames(20)
    H.snap("title")
    local px = read32(4)
    H.waitFrames(20)
    H.assertEq(read32(4), px, "world parked behind the title", 14)

    -- a fresh START press enters Zone 1's first room (a full reload)
    local f0 = H.dbgU16(2)
    H.padScript(function(f) return { start = (f - f0) > 4 and (f - f0) < 10 } end)
    H.waitUntil(function()
        return H.dbgU8(18) == 3 and H.dbgU8(23) == 1 and read32(4) == 40 * 256
    end, 300, "START -> room03 (the zone start)", 15)
    H.padScript(nil)
    H.waitFrames(60) -- the room id/checksum/px land MID-transition; the
                     -- tail (ent_room_init) runs ~25 blocked frames later
                     -- (D-017's 40-60-frame transition — probe-measured
                     -- again here; a 20-frame margin read stale entities)
    H.assertEq(H.dbgU8(21), 0, "room03 spawns nothing", 16)
    H.snap("title_dismissed")

    H.assertEq(H.dbgU8(22), 0, "no queue overflow", 19)
    H.assertEq(H.dbgU16(34), 0, "no lag frames", 19)
    H.pass()
end)
