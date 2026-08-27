-- Headless Mesen capture of the navigation cue, command $0F.
--
-- $2AB5 is `ld a,$0F / jp $2F88`.  Bank 2 $411D calls it every time the roster
-- selector moves between players, and fixed $02E9 calls the same routine when
-- the title screen toggles the player count -- so the title is a much cheaper
-- place to provoke the identical cue.
--
-- $2F88 maps command $0F through $2FB0 to program $07 with a $19-frame
-- priority window, and $30A0 starts it by writing the index into $DD72 with
-- bit 7 set.  This logs every APU write from that moment.

local CAPTURE_FRAMES = tonumber(os.getenv("ALLSTAR_SFX_FRAMES") or "") or 60

local mem = emu.memType.gameboyDebug
local totalFrames = 0
local sfxFrame = -1
local armed = false
local stopping = false
local events = {}
local failures = 0
local programByte = nil

local function read(address)
  return emu.read(address, mem, false)
end

local function expect(condition, message)
  if not condition then
    failures = failures + 1
    print("TRACE ERROR: " .. message)
  end
end

-- $2AB5 has just been entered: the cue is about to be requested.
local function onNavigationCue()
  if armed then return end
  armed = true
  sfxFrame = 0
  print(string.format("NAV_CUE frame=%d", totalFrames))
end

-- $2FA8 has stored the program byte and the priority window.
local function onSoundSelected()
  if not armed or programByte ~= nil then return end
  programByte = read(0xDD72)
  expect(programByte == 0x87,
    string.format("$2FA5 wrote $%02X to $DD72, expected $87", programByte))
  expect(read(0xC193) == 0x0F, "$2F9B did not latch command $0F")
  expect(read(0xC194) == 0x19, "$2FA9 did not install the $19 window")
  print(string.format("NAV_PROGRAM $DD72=%02X command=%02X window=%02X",
    programByte, read(0xC193), read(0xC194)))
end

local function onApuWrite(address, value)
  if not armed or stopping then return end
  if sfxFrame < 0 or sfxFrame > CAPTURE_FRAMES then return end
  events[#events + 1] = string.format("APU f=%d %04X=%02X",
    sfxFrame, address, value)
end

local function onInputPolled()
  local input = {
    a = false, b = false, start = false, select = false,
    up = false, down = false, left = false, right = false
  }
  -- Let the title settle, then toggle the player count once.
  if totalFrames == 320 then input.right = true end
  emu.setInput(input, 0)
end

local function onEndFrame()
  totalFrames = totalFrames + 1
  if armed and not stopping then sfxFrame = sfxFrame + 1 end
  if stopping then return end
  if armed and sfxFrame >= CAPTURE_FRAMES then
    stopping = true
    print(string.format("NAV_SFX_CAPTURE frames=%d events=%d",
      CAPTURE_FRAMES, #events))
    for i = 1, #events do print(events[i]) end
    print(failures == 0 and
      "TRACE PASSED: $2AB5 command $0F navigation cue" or
      string.format("TRACE FAILED: %d mismatch(es)", failures))
    emu.stop(failures == 0 and 0 or 3)
  elseif totalFrames > 900 then
    stopping = true
    print("TRACE ERROR: the navigation cue never fired")
    emu.stop(2)
  end
end

emu.addMemoryCallback(onNavigationCue, emu.callbackType.exec,
  0x2AB5, 0x2AB5, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onSoundSelected, emu.callbackType.exec,
  0x2FAC, 0x2FAC, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onApuWrite, emu.callbackType.write,
  0xFF10, 0xFF3F, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addEventCallback(onInputPolled, emu.eventType.inputPolled)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
