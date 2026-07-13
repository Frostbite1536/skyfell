-- test_title — Phase 3.5 (D-019): the title card (release boots into it;
-- TEST builds summon it via verb 10 so the harness never stalls) parks
-- the world, renders console text, and START enters the hall through the
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

    -- START enters the hall at the spawn (a full reload)
    H.padScript(function() return { start = true } end)
    H.waitUntil(function()
        return H.dbgU8(23) == 1 and read32(4) == 88 * 256
    end, 300, "START -> the hall", 15)
    H.padScript(nil)
    H.waitFrames(20)
    H.assertEq(H.dbgU8(21), 2, "entities live after dismiss", 16)
    H.snap("title_dismissed")

    H.assertEq(H.dbgU8(22), 0, "no queue overflow", 19)
    H.assertEq(H.dbgU16(34), 0, "no lag frames", 19)
    H.pass()
end)
