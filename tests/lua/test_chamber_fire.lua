-- test_chamber_fire — Phase 3: the Rift Gun works INSIDE the chamber.
-- Jeremy's spin-feel gate needs this loop in his hands: R aim-lock freezes
-- movement (D-013), Y fires blue / X fires gold as SCREEN directions
-- (cham_fire rotates them into the world through the gravity frame), the
-- shots place portals on chamber brass, the fired pair drives a REAL
-- transit -> gravity flip, and Select recalls both.
-- Chamber facts: floor y=736 (brass tiles 56-71), right wall x=736 (brass
-- rows 56-71), ledge B at px 592-672 / top y=560. Stop codes 10..11, 14..19.

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

H.run(function()
    H.waitAlive(10)
    H.warp(1, 14)
    H.waitUntil(function() return H.dbgU8(18) == 1 end, 120, "chamber", 14)

    -- (a) aim-lock: R+Right for 30 frames must not move Wren (D-013)
    posset(500, 706)
    local ax = read32(4)
    H.padScript(function() return { r = true, right = true } end)
    H.waitFrames(30)
    H.assertEq(read32(4), ax, "aim-lock holds Wren still", 15)
    H.padScript(nil)
    H.waitFrames(4)

    -- (b) Y fires BLUE east (default facing aim): from ledge B the shot
    -- crosses to the right wall's brass (hit row 68 -> strip rows 65-70,
    -- outward normal left = orient 3)
    posset(640, 530)
    local f0 = H.dbgU16(2)
    H.padScript(function(f) return { y = (f - f0) < 4 } end)
    H.waitUntil(function() return H.dbgU8(19) == 0x83 end, 120,
                "blue placed on the right wall", 16)
    H.padScript(nil)

    -- (c) R+Down aims screen-down, X fires GOLD into the floor brass under
    -- Wren's own feet (hit col 63 -> strip 60-65, orient 0 — standing on
    -- the target is legal, the validator only rejects POOL entities)
    posset(500, 706)
    f0 = H.dbgU16(2)
    H.padScript(function(f)
        local d = f - f0
        return { r = true, down = true, x = (d >= 8 and d < 12) }
    end)
    H.waitUntil(function() return H.dbgU8(20) == 0x80 end, 120,
                "gold placed in the floor", 17)
    H.padScript(nil)
    H.snap("fire_pair")

    -- (d) the floor opens under him: transit through the fired pair flips
    -- gravity to the right wall (exit orient 3 -> grav 3)
    H.waitUntil(function() return H.dbgU8(16) == 3 end, 240, "gravity flip", 18)
    -- hold Select: the tween blocks input, so the recall lands on the
    -- resume frame — before Wren can fall back into the exit portal
    H.padScript(function() return { select = true } end)
    -- cleared = bit7 down (the mirror keeps the old orient in bits 0-1)
    H.waitUntil(function()
        return H.dbgU8(17) ~= 5 and H.dbgU8(19) < 0x80 and H.dbgU8(20) < 0x80
    end, 240, "player recall after the spin", 18)
    H.padScript(nil)
    H.waitUntil(function()
        return H.dbgU8(17) ~= 5 and math.floor(read32(4) / 256) == 706
    end, 300, "land on the right wall", 18)
    H.waitFrames(10)
    H.snap("fire_landed")

    H.assertEq(H.dbgU8(22), 0, "no queue overflow", 19)
    H.assertEq(H.dbgU16(34), 0, "no lag frames", 19)
    H.pass()
end)
