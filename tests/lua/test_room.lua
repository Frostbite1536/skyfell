-- test_room — the room pipeline + ALL FOUR camera streaming directions,
-- driven by the real player (coverage gap closed per the Phase 1 review:
-- push_row and leftward push_col were previously untested):
--   (a) boot: checksum valid
--   (b) RIGHTWARD live streaming: run+jump right past the rung girder, the
--       step block, and the pit, up to the brass tower (~x 822, cam ~691);
--       the tower tiles must have entered the window live
--   (c) LEFTWARD live streaming: walk+jump back to the spawn wall; the step
--       block must be re-streamed into its slot live
--   (d) VERTICAL (row) streaming: warp high, fall ~300px; the camera follows
--       down and must stream the floor rows live (the push_row screen-split)
-- World facts (assets/maps/room01.txt): brass tower meta (52,24) -> tile
-- (104,48) word 0x0009, slot addr 0x3400+(16<<5)+8 = 0x3608 (screen1).
-- Step block meta (26,26) -> tile (52,52) word 0x0001, slot addr
-- 0x3400+(20<<5)+20 = 0x3694. Floor meta (10,28) -> tile (20,56) word
-- 0x0001, slot addr 0x3000+(24<<5)+20 = 0x3314. Each slot holds a DIFFERENT
-- world tile before its leg runs, so only live streaming produces the value.
-- Stop codes 10..11, 14..19.

H.maskInput()
local function posset(x, y)
    H.writeU16(DBG_BASE + 38, x)
    H.writeU16(DBG_BASE + 40, y)
    H.writeU16(DBG_BASE + 36, 4)
    H.waitUntil(function() return H.dbgU16(36) == 0 end, 60, "posset ack", 11)
    H.waitFrames(8)
end

H.run(function()
    H.waitAlive(10)
    H.assertEq(H.dbgU8(23), 1, "room checksum", 14)

    -- (b) right with a full-height jump every 48 frames
    H.padScript(function(f)
        return { right = true, b = (f % 48) < 24 }
    end)
    H.waitUntil(function() return H.dbgU16(44) >= 600 end, 1200,
                "camera follow right to the tower", 15)
    H.padScript(nil)
    H.waitFrames(6)
    H.assertEq(H.vramWord(0x3608), 0x0009, "brass tower streamed (rightward)", 16)

    -- (c) left back to the spawn wall
    H.padScript(function(f)
        return { left = true, b = (f % 48) < 24 }
    end)
    H.waitUntil(function() return H.dbgU16(44) <= 60 end, 1500,
                "camera follow left home", 15)
    H.padScript(nil)
    H.waitFrames(6)
    H.assertEq(H.vramWord(0x3694), 0x0001, "step block streamed (leftward)", 17)
    H.snap("room_home")

    -- (d) vertical: warp high (window rows 0..31), fall onto the first rung;
    -- the camera follows down ~277px and must stream rows 32..63 live
    posset(200, 80)
    H.waitUntil(function() return H.dbgU16(46) >= 270 end, 300,
                "camera follow the fall down", 15)
    H.waitFrames(6)
    H.assertEq(H.vramWord(0x3314), 0x0001, "floor row streamed (vertical)", 18)
    H.snap("room_fall")

    H.assertEq(H.dbgU8(22), 0, "vq overflow across all streaming", 19)
    H.assertEq(H.dbgU16(34), 0, "lag frames across all streaming", 19)
    H.pass()
end)
