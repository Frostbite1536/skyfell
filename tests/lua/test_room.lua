-- test_room — the room pipeline + camera streaming (D-012):
--   (a) boot state: checksum valid, corner tile in VRAM
--   (b) LIVE seam streaming: fly the free camera to the bottom-right corner
--       with held d-pad input; the brass pad tiles must have been streamed
--       into the window on the way (no warp, no force-blank)
--   (c) POS_SET warp back to the origin redraws the window whole
-- World facts (assets/maps/room01.txt): tile (0,0)=hull tl (word 0x0001);
-- brass pad at metatile (46,28) -> tile (92,56) = brass tl (word 0x0009),
-- VRAM slot (92&63=28, 56&31=24) -> word addr 0x3000+(24<<5)+28 = 0x331C.
-- Stop codes 10..11, 14..19.

H.maskInput()
H.run(function()
    H.waitAlive(10)
    H.assertEq(H.dbgU8(23), 1, "room checksum", 14)

    -- (b) hold right+down: free camera flies to the clamp corner (768,288)
    H.padScript(function(f) return { right = true, down = true } end)
    H.waitUntil(function()
        return H.dbgU16(44) == 768 and H.dbgU16(46) == 288
    end, 600, "camera reach clamp corner", 15)
    H.padScript(nil)
    H.waitFrames(4)

    -- streamed brass pad tile present (live pushes, never a redraw)
    H.assertEq(H.vramWord(0x331C), 0x0009, "brass tl streamed", 16)
    -- queue stayed healthy through ~400 frames of streaming
    H.assertEq(H.dbgU8(22), 0, "vq overflow during streaming", 17)
    H.assertEq(H.dbgU16(34), 0, "lag frames during streaming", 18)
    H.snap("room_brass_corner")

    -- (c) warp home: full force-blank redraw restores the origin window
    H.writeU16(DBG_BASE + 38, 0)
    H.writeU16(DBG_BASE + 40, 0)
    H.writeU16(DBG_BASE + 36, 4) -- POS_SET(0,0)
    H.waitUntil(function() return H.dbgU16(36) == 0 end, 60, "warp ack", 11)
    H.waitFrames(4)
    H.assertEq(H.vramWord(0x3000), 0x0001, "corner word after warp", 19)
    H.snap("room_origin")

    H.pass()
end)
