-- test_room2 — Phase 3.5 unit A: rooms beyond room 0 are REAL (D-016).
-- Warp to room02: its OWN checksum verifies the WRAM live-map copy
-- (INV-ENG-005 against ROOM02_CKSUM — a wrong or torn copy fails), its
-- authored spawns differ from room01's, physics runs on its geometry,
-- a portal places on its brass, and warping back to room01 reloads
-- clean. Stop codes 10..11, 14..19.

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

    H.warp(2, 14)
    H.waitUntil(function() return H.dbgU8(18) == 2 end, 120, "room02", 14)
    H.assertEq(H.dbgU8(23), 1, "room02 checksum (live-map copy)", 14)

    -- authored spawns: crate slot 0 at (160,450) — room01's is (320,434)
    H.waitFrames(2)
    H.assertEq(H.dbgU8(21), 2, "room02 entities", 15)
    verb(7, 0, 0)
    H.waitFrames(2)
    H.assertEq(H.dbgU16(50), 160, "room02 crate x", 15)
    H.assertEq(H.dbgU16(52), 450, "room02 crate y", 15)

    -- physics on room02 geometry: spawn (88,434) stands on the floor;
    -- 30 held frames of right must move Wren (no golden — room01's
    -- runway goldens + replay own determinism)
    local x0 = read32(4)
    local f0 = H.dbgU16(2)
    H.padScript(function(f) return { right = (f - f0) < 30 } end)
    H.waitFrames(40)
    H.padScript(nil)
    if read32(4) <= x0 then
        H.fail("Wren did not walk on room02's floor", 16)
    end

    -- the validator + overlay run against the live copy: blue on the
    -- brass floor pad (tiles 82-97 row 58; hit 86 -> strip inside)
    verb(6, 0 + 0 * 256, 86 + 58 * 256)
    H.assertEq(H.dbgU8(19), 0x80, "blue placed on room02 brass", 17)
    H.snap("room2")
    verb(6, 0xFFFF, 0)

    -- round-trip: room01 reloads clean over the live map
    H.warp(0, 18)
    H.waitUntil(function() return H.dbgU8(18) == 0 end, 120, "room01 back", 18)
    H.assertEq(H.dbgU8(23), 1, "room01 checksum after round-trip", 18)
    H.waitFrames(2)
    verb(7, 0, 0)
    H.waitFrames(2)
    H.assertEq(H.dbgU16(50), 320, "room01 crate restored", 18)

    H.assertEq(H.dbgU8(22), 0, "no queue overflow", 19)
    H.assertEq(H.dbgU16(34), 0, "no lag frames", 19)
    H.pass()
end)
