-- test_chamber_drone — Phase 3: the Gale Drone lives in the chamber.
-- It spawns with the chamber (slot 0), drifts inside its D-014 leash box
-- (spawn center 512,500 +- DRONE_R=56 — authored clear of every ledge and
-- brass-strip front, so it can never fizzle a scripted portal), FREEZES
-- while the rotation tween runs (the world freeze; its sprite hides —
-- sprites can't rotate, INV-HW-004), resumes after, and the whole patrol
-- is deterministic across chamber reloads (INV-ENG-002).
-- Stop codes 10..11, 14..19.

H.maskInput()
local function verb(c, a0, a1)
    H.writeU16(DBG_BASE + 38, a0)
    H.writeU16(DBG_BASE + 40, a1)
    H.writeU16(DBG_BASE + 36, c)
    H.waitUntil(function() return H.dbgU16(36) == 0 end, 150, "verb ack", 11)
end

H.run(function()
    H.waitAlive(10)
    H.warp(1, 14) -- enter the chamber
    H.waitUntil(function() return H.dbgU8(18) == 1 end, 120, "chamber", 14)
    H.waitFrames(2) -- dbg_entn updates on the first chamber ent_update
    H.assertEq(H.dbgU8(21), 1, "the drone spawned", 14)
    verb(7, 0, 0) -- watch slot 0 (chamber_load spawns the drone first)
    H.waitFrames(2)

    -- it moves, and its box top-left stays inside the leash
    -- (center 512,500 +-56, box 16x16 -> TL x 448..560, y 436..548;
    -- +-2 px margin for the crossing frame before the bounce)
    local x0 = H.dbgU16(50)
    local moved = false
    for _ = 1, 30 do
        H.waitFrames(10)
        local x = H.dbgU16(50)
        local y = H.dbgU16(52)
        if x ~= x0 then moved = true end
        if x < 446 or x > 562 or y < 434 or y > 550 then
            H.fail(string.format("drone left the leash: x=%d y=%d", x, y), 15)
        end
    end
    if not moved then H.fail("drone never moved", 15) end

    -- bring Wren up beside the patrol box so the drone is on screen
    verb(4, 500, 560)
    H.waitFrames(3)
    H.snap("drone_patrol")

    -- rotation: the world freezes -> the drone freezes (and hides)
    verb(9, 1, 0)
    H.waitUntil(function() return H.dbgU8(17) == 5 end, 30, "rotating", 16)
    local fx = H.dbgU16(50)
    local fy = H.dbgU16(52)
    H.waitFrames(8) -- the tween runs 32 frames (64 theta units / ROT_STEP 2)
    H.assertEq(H.dbgU8(17), 5, "still rotating", 16)
    H.assertEq(H.dbgU16(50), fx, "frozen x through the tween", 16)
    H.assertEq(H.dbgU16(52), fy, "frozen y through the tween", 16)
    H.snap("drone_hidden_midspin")
    H.waitUntil(function() return H.dbgU8(17) ~= 5 end, 120, "tween ends", 17)
    H.waitFrames(12)
    if H.dbgU16(50) == fx and H.dbgU16(52) == fy then
        H.fail("drone did not resume after the spin", 17)
    end

    -- determinism: reload the chamber twice, sample the same instant
    local function track()
        H.warp(0, 18) -- out to the gantry hall (full chamber reset)
        H.warp(1, 18)
        H.waitUntil(function() return H.dbgU8(18) == 1 end, 120, "re-enter", 18)
        verb(7, 0, 0)
        H.waitFrames(100)
        return string.format("x=%d y=%d vx=%d vy=%d",
                             H.dbgU16(50), H.dbgU16(52),
                             H.dbgU16(54), H.dbgU16(56))
    end
    local a = track()
    local b = track()
    if a ~= b then
        H.fail("patrol not deterministic:\nA: " .. a .. "\nB: " .. b, 18)
    end

    H.assertEq(H.dbgU8(22), 0, "no queue overflow", 19)
    H.assertEq(H.dbgU16(34), 0, "no lag frames", 19)
    H.pass()
end)
