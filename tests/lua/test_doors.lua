-- test_doors — Phase 3.5 unit B (D-017): doorway transitions. UP while
-- standing in a doorway fades out (NMI brightness shadow), force-blank
-- loads the linked room, fades in at the authored entry point; the
-- chamber's puzzle recess exits back to the hall the same way.
-- Doors are UP-gated so no other scripted test can trip one by walking.
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
local function press_up()
    local f0 = H.dbgU16(2)
    H.padScript(function(f) return { up = (f - f0) < 4 } end)
end

H.run(function()
    H.waitAlive(10)

    -- (a) hall west door -> room02 (arrive by its east door)
    posset(24, 418)
    press_up()
    H.waitUntil(function() return H.dbgU8(18) == 2 end, 300, "to room02", 14)
    H.padScript(nil)
    H.waitFrames(60) -- the load (map copy + force-blank redraw) runs tens
                     -- of frames inside the transition, then the fade-in;
                     -- probe-verified fully bright by +40
    H.assertEq(read32(4), 960 * 256, "room02 entry x", 14)
    H.snap("door_room02")

    -- (b) walk into room02's east doorway, UP -> back to the hall
    local f0 = H.dbgU16(2)
    H.padScript(function(f) return { right = (f - f0) < 30 } end)
    H.waitFrames(40)
    press_up()
    H.waitUntil(function() return H.dbgU8(18) == 0 end, 300, "back to hall", 15)
    H.padScript(nil)
    H.waitFrames(12)
    H.assertEq(read32(4), 40 * 256, "hall entry x", 15)

    -- (c) the pit door -> the chamber
    posset(616, 450)
    press_up()
    H.waitUntil(function() return H.dbgU8(18) == 1 end, 300, "to chamber", 16)
    H.padScript(nil)
    H.waitFrames(12)
    H.assertEq(H.dbgU8(16), 0, "chamber gravity down", 16)

    -- (d) the chamber's REAL exit: open the door by verbs (the full solve
    -- is test_chamber_puzzle's), walk into the recess, fade to the hall
    verb(9, 2, 0) -- gravity up
    H.waitUntil(function() return H.dbgU8(17) ~= 5 end, 200, "spin done", 17)
    verb(5, 1, 23 + 19 * 256) -- crate at (368,304): falls up onto the pad
    H.waitUntil(function()
        return H.vramWord(36 * 128 + 34) % 256 == 0
    end, 200, "the exit door opens", 17)
    posset(300, 288)
    H.padScript(function() return { right = true } end) -- world -x at g=2
    H.waitUntil(function() return H.dbgU16(68) == 1 end, 400,
                "the recess reached (end card shows, D-019)", 18)
    H.waitFrames(10)
    H.snap("end_card")
    H.padScript(function() return { start = true } end)
    H.waitUntil(function() return H.dbgU8(18) == 0 end, 300,
                "START returns to the hall", 18)
    H.padScript(nil)
    H.waitFrames(60)
    H.assertEq(read32(4), 256 * 256, "hall re-entry x", 18)
    H.snap("door_back_home")

    H.assertEq(H.dbgU8(22), 0, "no queue overflow", 19)
    H.assertEq(H.dbgU16(34), 0, "no lag frames", 19)
    H.pass()
end)
