-- test_audio — Phase 3.5 audio spike gate (D-020, R3):
--   (a) the SNESMOD driver is resident in ARAM after boot (real SPC bytes,
--       not a hung handshake),
--   (b) the module is actually DRIVING the DSP (register file changes over
--       time — music is playing, not just loaded),
--   (c) SFX wiring: a scripted jump plays JUMP then LAND; a mailbox portal
--       placement plays OPEN (ids via dbg_audio's high byte),
--   (d) all of it costs zero lag frames (spcProcess in the frame budget).
-- Driver-byte signature is pinned to pvsneslib 4.5.0's sm-spc (the
-- toolchain is pinned; a bump is a DECISIONS.md entry anyway).
-- Stop codes 10..19.

H.maskInput()

local function dsp_snapshot()
    local s = {}
    for r = 0, 127 do s[#s + 1] = emu.read(r, emu.memType.spcDspRegisters) end
    return table.concat(s, ",")
end

H.run(function()
    H.waitAlive(10)

    -- (a) audio ready + driver resident
    H.assertEq(H.dbgU16(72) % 256, 1, "audio ready flag (+72 low8)", 10)
    local sig = { 0xcd, 0x00, 0xe8, 0x00 } -- sm-spc init @ $0400 (4.5.0)
    for i = 1, 4 do
        H.assertEq(emu.read(0x400 + i - 1, emu.memType.spcRam), sig[i],
                   "SNESMOD driver byte " .. i .. " @ ARAM $0400", 11)
    end

    -- (b) the DSP register file must move while the module plays
    local before = dsp_snapshot()
    H.waitFrames(60)
    if dsp_snapshot() == before then
        H.fail("DSP registers frozen for 60 frames - module not playing", 14)
    end

    -- (c) SFX: jump -> JUMP + LAND on the counter, LAND (id 4) last
    local base = H.dbgU16(74)
    local f0 = H.dbgU16(2)
    H.padScript(function(f) return { b = (f - f0) < 20 } end)
    H.waitUntil(function() return H.dbgU16(74) >= base + 2 end, 120,
                "jump+land sfx", 15)
    H.padScript(nil)
    H.assertEq(math.floor(H.dbgU16(72) / 256), 5, "LAND was the last sfx", 16)

    -- portal placement through THE validator (verb 6) -> OPEN
    -- (the floor pad tx 96 ty 56 — test_portal_rules' known-good target)
    base = H.dbgU16(74)
    H.writeU16(DBG_BASE + 38, 0)            -- blue, orient 0
    H.writeU16(DBG_BASE + 40, 96 + 56 * 256)
    H.writeU16(DBG_BASE + 36, 6)
    H.waitUntil(function() return H.dbgU16(36) == 0 end, 60, "portal ack", 17)
    H.assertEq(H.dbgU8(19), 0x80, "placement actually landed", 17)
    H.assertEq(H.dbgU16(74), base + 1, "portal-open sfx", 18)
    H.assertEq(math.floor(H.dbgU16(72) / 256), 3, "OPEN was the last sfx", 18)

    -- (d) the whole run held 60fps
    H.assertEq(H.dbgU16(34), 0, "no lag frames", 19)
    H.pass()
end)
