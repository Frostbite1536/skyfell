-- SKYFELL emulator-test harness (MesenCE Lua).
-- tools/run_tests.py concatenates this file ahead of each test_*.lua, so tests
-- use the H.* helpers directly — no require/dofile (the sandbox has no stable
-- script dir and io/os are disabled by default).
--
-- API ground truth (MesenCE 2.2.1 source, Core/Debugger/LuaApi.cpp):
--   emu.read/read16(addr, emu.memType.snesMemory), emu.stop(code),
--   emu.takeScreenshot() -> PNG string, emu.addEventCallback(fn, emu.eventType.endFrame)

DBG_BASE = 0x7EFF00 -- debug block base (dbg.asm, D-010); offsets are the contract in tests/README.md

H = {}
local co = nil
local passed = false -- set by H.pass; guards the dead-coroutine 13 stamp

function H.readU8(a) return emu.read(a, emu.memType.snesMemory) end
function H.readU16(a) return emu.read16(a, emu.memType.snesMemory) end
function H.dbgU8(off) return H.readU8(DBG_BASE + off) end
function H.dbgU16(off) return H.readU16(DBG_BASE + off) end

function H.writeU16(a, v)
    emu.write(a, v % 256, emu.memType.snesMemory)
    emu.write(a + 1, math.floor(v / 256) % 256, emu.memType.snesMemory)
end

-- VRAM word read: Mesen's snesVideoRam is byte-addressed; SNES VRAM
-- addresses are words.
function H.vramWord(waddr) return emu.read16(waddr * 2, emu.memType.snesVideoRam) end

-- CGRAM word read: 512-byte palette RAM, 2 bytes per color (BGR555). Entry i
-- (0..255) is at byte offset i*2. Callers nil-guard on Mesen builds lacking
-- snesCgRam.
function H.cgramWord(entry) return emu.read16(entry * 2, emu.memType.snesCgRam) end

-- Spin emulated frames until pred() is true; fail with `code` on timeout.
function H.waitUntil(pred, maxframes, what, code)
    for _ = 1, maxframes do
        if pred() then return end
        coroutine.yield()
    end
    H.fail((what or "waitUntil") .. " timed out after " .. maxframes .. " frames", code)
end

-- Wait for the TEST build to come alive (dbg fields are WRAM garbage until
-- dbg_magic goes live — main.c writes it last).
function H.waitAlive(code)
    H.waitUntil(function() return H.dbgU16(0) == 0x51FE end, 300, "dbg_magic", code)
end

-- Warp-request mailbox (tests/README.md, +24): Lua writes room id + 0x8000;
-- the game consumes by writing 0 back. Phase 1 wires the game side.
function H.warp(room, code)
    H.writeU16(DBG_BASE + 24, room + 0x8000)
    H.waitUntil(function() return H.dbgU16(24) == 0 end, 300, "warp ack", code)
end

-- Yield n emulated frames (H.run resumes the test body once per endFrame).
function H.waitFrames(n)
    for _ = 1, n do
        coroutine.yield()
    end
end

-- ==== Scripted pads ==============================================
-- Ground truth (MesenCE 2.2.1 source, learned the hard way in prophet):
-- BaseControlManager::UpdateInputState applies host input, THEN fires
-- EventType::InputPolled — so emu.setInput() inside an inputPolled callback
-- is what the auto-joypad latches and the pvsneslib ISR reads. Button names
-- from SnesController::GetKeyNameAssociations (lowercase).
--
-- setInput ARGUMENT ORDER (LuaApi::SetInput): the real signature is
-- emu.setInput(table, SUBPORT, PORT) — PORT is the THIRD argument. Calling
-- emu.setInput(table, port) silently targets port 0 (nil 3rd arg -> 0);
-- that clobbered prophet's dual-pad tests (their D-047). Always pass the
-- port as the 3rd arg when driving port 1.
--
-- H.maskInput() arms the callback with every button forced false — host
-- keyboard input can never leak into a run (INV-ENG-002 insurance). Every
-- test should call it first. H.padScript(fn) supplies inputs as a function
-- of the game's own frame counter (dbg_frame, +2): deterministic and
-- self-synchronizing — no frame-offset bookkeeping to drift.
local PAD_KEYS = { "a", "b", "x", "y", "l", "r", "up", "down",
                   "left", "right", "select", "start" }
local padFn = nil
local maskArmed = false

-- Build a full 12-button setInput table from a (possibly nil) source table.
local function padTable(src)
    local t = {}
    for _, k in ipairs(PAD_KEYS) do
        t[k] = (src ~= nil and src[k]) and true or false
    end
    return t
end

function H.maskInput()
    if maskArmed then return end
    maskArmed = true
    emu.addEventCallback(function()
        local src = nil
        if padFn ~= nil then
            src = padFn(H.dbgU16(2)) -- the game's own frame counter
        end
        -- Single-pad game: port 0 only. emu.setInput(t, 0) targets port 0
        -- (nil 3rd arg -> 0).
        emu.setInput(padTable(src), 0)
    end, emu.eventType.inputPolled)
end

function H.padScript(fn)
    padFn = fn
end

-- In testrunner mode emu.log stays in the (invisible) script log; displayMessage
-- reaches stdout when --enablestdout is set. Distinct stop codes pinpoint the
-- failing assert even if messages are swallowed: rc 0 pass, 1 generic fail,
-- 12 lua error, 13 body-ended-without-pass; tests pass their own codes (10+,
-- avoiding 12/13).
function H.fail(msg, code)
    emu.displayMessage("test", "FAIL: " .. tostring(msg))
    emu.log("FAIL: " .. tostring(msg))
    -- The testrunner swallows log output; persist the detail next to the
    -- artifacts so a failure names itself (io is enabled in our Mesen).
    if io ~= nil then
        local f = io.open((OUTDIR or ".") .. "/fail.txt", "w")
        if f then
            f:write("FAIL(rc=" .. tostring(code or 1) .. "): " .. tostring(msg) .. "\n")
            f:close()
        end
    end
    emu.stop(code or 1)
    -- emu.stop() latches the exit code but does NOT unwind this Lua call:
    -- without parking here, the body runs on past the failed assert and a
    -- later error overwrites fail.txt with a misleading code.
    while true do
        coroutine.yield()
    end
end

function H.pass()
    emu.displayMessage("test", "PASS")
    emu.log("PASS")
    passed = true
    emu.stop(0)
    -- Park, exactly like H.fail: emu.stop LATCHES the code but the emulator
    -- halts asynchronously. Without parking, the body returns, the coroutine
    -- goes "dead", and if one more endFrame slips in before the halt, the
    -- dead-coroutine guard in H.run stamps rc=13 OVER a clean pass (prophet's
    -- intermittent rc=13 flake, found 2026-07-09).
    while true do
        coroutine.yield()
    end
end

function H.assertEq(got, want, what, code)
    if got ~= want then
        H.fail(string.format("%s: got 0x%X, want 0x%X", what or "assertEq", got, want), code)
    end
end

-- Screenshot into the runner-controlled cwd (artifacts/<test>/). Degrades to a
-- log line if the io sandbox is closed; the run still gates on assertions.
function H.snap(name)
    local png = emu.takeScreenshot()
    local path = (OUTDIR or ".") .. "/" .. name .. ".png"
    if io ~= nil then
        local f = io.open(path, "wb")
        if f then
            f:write(png)
            f:close()
            emu.log("SNAP: " .. path .. " (" .. #png .. " bytes)")
            return
        end
    end
    emu.log("SNAP-SKIPPED: io sandbox closed (" .. #png .. " bytes captured)")
end

-- Run a test body as a coroutine resumed once per frame. The body must end by
-- calling H.pass() or H.fail(...).
function H.run(testBody)
    co = coroutine.create(testBody)
    emu.addEventCallback(function()
        if coroutine.status(co) == "dead" then
            -- belt for the H.pass park: never let a passed run re-stop as 13
            if not passed then
                H.fail("test body ended without calling H.pass()", 13)
            end
            return
        end
        local ok, err = coroutine.resume(co)
        if not ok then
            H.fail("lua error: " .. tostring(err), 12)
        end
    end, emu.eventType.endFrame)
end
