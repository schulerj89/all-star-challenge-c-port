-- Headless Mesen capture of the title-screen music, $029C -> $DD73 = $81.
--
-- $02A7 posts $81 into the sound-command mailbox $DD73.  The frame handler at
-- $3014 sees bit 7, takes song index $01, and from then on runs the four music
-- voices through $3264 -> $347B.  This script logs every APU register write the
-- cartridge makes, tagged with the audio frame it happened on, so the port's
-- decoded song can be diffed against the hardware program rather than against
-- a reading of the disassembly.
--
-- No input is pressed: the title holds for $0960 frames before attract, which
-- is far longer than this capture.

local CAPTURE_FRAMES = tonumber(os.getenv("ALLSTAR_TITLE_FRAMES") or "") or 900

local mem = emu.memType.gameboyDebug
local totalFrames = 0
local audioFrame = -1
local songStarted = false
local stopping = false
local events = {}
local failures = 0

local function read(address)
  return emu.read(address, mem, false)
end

local function expect(condition, message)
  if not condition then
    failures = failures + 1
    print("TRACE ERROR: " .. message)
  end
end

-- $02A9 has just written the command byte.
local function onTitleCommand()
  if songStarted then return end
  local command = read(0xDD73)
  if command ~= 0x81 then return end
  songStarted = true
  audioFrame = -1
  print(string.format("TITLE_COMMAND frame=%d $DD73=%02X", totalFrames, command))
end

-- $3014 is the once-per-frame sound handler.
local function onAudioFrame()
  if not songStarted then return end
  audioFrame = audioFrame + 1
end

local function onApuWrite(address, value)
  if not songStarted or stopping then return end
  if audioFrame < 0 then return end
  if audioFrame > CAPTURE_FRAMES then return end
  events[#events + 1] = string.format("APU f=%d %04X=%02X",
    audioFrame, address, value)
end

local function onEndFrame()
  totalFrames = totalFrames + 1
  if stopping then return end
  if songStarted and audioFrame >= CAPTURE_FRAMES then
    stopping = true
    -- The engine consumed the command and latched song 1's parameters.
    expect(read(0xDD73) == 0x01, "$30A0 did not latch song index $01")
    expect(read(0xDD77) == 0x07, "$386F+1 update skip was not $07")
    print(string.format("TITLE_MUSIC_CAPTURE frames=%d events=%d",
      CAPTURE_FRAMES, #events))
    for i = 1, #events do print(events[i]) end
    print(failures == 0 and
      "TRACE PASSED: $029C title music APU program" or
      string.format("TRACE FAILED: %d mismatch(es)", failures))
    emu.stop(failures == 0 and 0 or 3)
  elseif totalFrames > 2000 then
    stopping = true
    print("TRACE ERROR: the title screen never posted $81 to $DD73")
    emu.stop(2)
  end
end

emu.addMemoryCallback(onTitleCommand, emu.callbackType.exec,
  0x02AC, 0x02AC, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onAudioFrame, emu.callbackType.exec,
  0x3014, 0x3014, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onApuWrite, emu.callbackType.write,
  0xFF10, 0xFF3F, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
