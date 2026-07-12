-- test_portal_rules — ROADMAP Phase 2 gate, all through THE validator
-- (INV-ENG-004, mailbox verb 6):
--   (a) non-brass surface rejected (hull floor)
--   (b) undersized brass rejected (the 2-metatile patch, tiles 66-69 row 56
--       — 4 tiles < the 6-tile strip even after sliding)
--   (c) valid placement on the floor pad works
--   (d) entity-occupied placement rejected (crate parked on the pad)
--   (e) refiring a color MOVES it (blue pad -> tower face)
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
    posset(700, 418) -- clear of every target surface
    local fizz0 = H.dbgU16(60)

    -- (a) hull is not brass
    verb(6, 0 + 0 * 256, 20 + 56 * 256)
    H.assertEq(H.dbgU8(19), 0, "hull placement refused", 14)
    H.assertEq(H.dbgU16(60), fizz0 + 1, "fizzle counted (a)", 14)

    -- (b) undersized brass patch (2 metatiles = 4 tiles < 6)
    verb(6, 0 + 0 * 256, 67 + 56 * 256)
    H.assertEq(H.dbgU8(19), 0, "undersized patch refused", 15)
    H.assertEq(H.dbgU16(60), fizz0 + 2, "fizzle counted (b)", 15)

    -- (c) the real pad takes it
    verb(6, 0 + 0 * 256, 96 + 56 * 256)
    H.assertEq(H.dbgU8(19), 0x80, "pad placement ok", 16)

    -- (d) recall, park a crate on the pad, try gold there -> occupied
    verb(6, 0xFFFF, 0)
    H.assertEq(H.dbgU8(19), 0, "recall cleared blue", 17)
    verb(5, 1, 48 + 27 * 256) -- ET_CRATE at metatile (48,27), lands on pad
    H.waitFrames(20)          -- let it settle + sleep
    verb(6, 1 + 0 * 256, 96 + 56 * 256)
    H.assertEq(H.dbgU8(20), 0, "entity-occupied refused", 17)
    H.assertEq(H.dbgU16(60), fizz0 + 3, "fizzle counted (d)", 17)

    -- (e) blue on the pad's clear half? no — prove REFIRE MOVES: place blue
    -- on the tower face, then refire blue at the pillar face
    verb(6, 0 + 3 * 256, 104 + 40 * 256)
    H.assertEq(H.dbgU8(19), 0x83, "blue on the tower", 18)
    verb(6, 0 + 3 * 256, 124 + 52 * 256)
    H.assertEq(H.dbgU8(19), 0x83, "blue MOVED to the pillar", 18)
    H.waitFrames(4)
    -- the tower cells redrew as raw brass: tile (104,40) = meta (52,20)
    -- interior column -> brass tl/bl? col 104 even, row 40 even -> tl = 9,
    -- slot (104&63=40 screen1, 40&31=8): 0x3400+(8<<5)+8 = 0x3508
    H.assertEq(H.vramWord(0x3508), 0x0009, "old cells restored", 19)

    H.snap("portal_rules")
    H.pass()
end)
