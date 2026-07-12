-- test_room — the room pipeline + camera streaming (D-012), driven by the
-- real player (Unit C):
--   (a) boot state: checksum valid
--   (b) LIVE seam streaming: walk Wren right along the floor (into the pit,
--       against its far wall); the camera deadzone-follows and must stream
--       the brass tower into the window on the way (no warp, no force-blank)
--   (c) POS_SET warp back to spawn redraws the window whole
-- World facts (assets/maps/room01.txt): brass tower metatile (52,24) ->
-- tile (104,48) = brass tl (word 0x0009); after walking right the window is
-- cols 53..116 / rows 32..63, so slot (104&63=40 -> screen1, 48&31=16) ->
-- word addr 0x3400+(16<<5)+8 = 0x3608. At boot that slot held world col 40
-- (empty sky) — only live streaming can make it brass.
-- Stop codes 10..11, 14..19.

H.maskInput()
H.run(function()
    H.waitAlive(10)
    H.assertEq(H.dbgU8(23), 1, "room checksum", 14)

    -- (b) run right with a full-height jump every 48 frames: clears the
    -- 32px step block at metatile 26, drops into the pit, jumps out of it
    -- (apex ~51px > 48px depth), reaches the far pit wall; camera deadzone-
    -- follows to ~547
    H.padScript(function(f)
        return { right = true, b = (f % 48) < 24 }
    end)
    H.waitUntil(function() return H.dbgU16(44) >= 500 end, 900,
                "camera follow player right", 15)
    H.padScript(nil)
    H.waitFrames(6)

    H.assertEq(H.vramWord(0x3608), 0x0009, "brass tower streamed live", 16)
    H.assertEq(H.dbgU8(22), 0, "vq overflow during streaming", 17)
    H.assertEq(H.dbgU16(34), 0, "lag frames during walk", 18)
    H.snap("room_pit_wall")

    -- (c) warp home: POS_SET(spawn) — full force-blank redraw + camera clamp
    H.writeU16(DBG_BASE + 38, 88)
    H.writeU16(DBG_BASE + 40, 418)
    H.writeU16(DBG_BASE + 36, 4)
    H.waitUntil(function() return H.dbgU16(36) == 0 end, 60, "warp ack", 11)
    H.waitFrames(6)
    H.assertEq(H.dbgU16(4), 88 * 256, "player x after warp (16.8 lo)", 19)
    H.assertEq(H.vramWord(0x3000), 0x0001, "corner word after warp", 19)
    H.assertEq(H.dbgU8(17), 0, "fsm idle on the floor", 19)
    H.snap("room_spawn")

    H.pass()
end)
