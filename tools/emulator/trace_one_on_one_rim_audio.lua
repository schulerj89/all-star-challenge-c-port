-- Headless Mesen proof for fixed $1CED->$1D8C->$1F5F->$2F88 command $09.
-- Forces one exact left-rim miss cell and captures the resulting DMG APU
-- register program so the native asset pack can reproduce the cartridge cue.

local totalFrames = 0
local gameplayFrames = 0
local gameplayReached = false
local rosterChooseCount = 0
local opponentMoved = false
local mem = emu.memType.gameboyDebug
local armed = false
local rimObserved = false
local commandObserved = false
local captureStart = nil
local events = {}
local lastRoute = nil
local failures = 0
local stopping = false

local function read(address)
  return emu.read(address, mem, false)
end

local function write(address, value)
  emu.write(address, value, mem)
end

local function setWord(address, value)
  write(address, value & 0xFF)
  write(address + 1, (value >> 8) & 0xFF)
end

local function expect(condition, message)
  if not condition then
    failures = failures + 1
    print("TRACE ERROR: " .. message)
  end
end

local function masterClock()
  return emu.getState()["masterClock"] or 0
end

local function onRosterChoose()
  rosterChooseCount = rosterChooseCount + 1
end

local function onPlayerUpdate()
  gameplayReached = true
end

local function onCourtDispatcher()
  if not gameplayReached or armed then return end
  armed = true
  -- $1D8C left-rim cell: X=$53, Y=$5E, Z=$37.  Clear the duplicate
  -- actor latch so $1F5F must dispatch command $09.
  write(0xFFCF, 0)
  write(0xFFD0, 1)
  write(0xC142, 0xFF)
  write(0xFF8B, 0x01)
  write(0xFFF8, 0x01)
  setWord(0xC0A0, 0x0000)
  setWord(0xC0A2, 0x5300)
  setWord(0xC0A4, 0x0000)
  setWord(0xC0A6, 0x5E00)
  setWord(0xC0A8, 0xFFB0)
  setWord(0xC0AA, 0x3700)
  write(0xC17E, 0)
  print("RIM_AUDIO_ARMED cell=53,5E,37 duplicate_actor=FF/01")
end

local function onRimReturn()
  if not armed or rimObserved then return end
  rimObserved = true
  expect(read(0xC17E) == 0x08,
    "$1F5F did not install the eight-frame rim cooldown")
  expect(read(0xFFF8) == 1,
    "$1F5F rim contact unexpectedly cleared initial-flight $FFF8")
  print(string.format(
    "RIM_CONTACT vx=%02X%02X cooldown=%02X first_flight=%02X",
    read(0xC0A1), read(0xC0A0), read(0xC17E), read(0xFFF8)))
end

local pendingCommand = nil
local function onSoundSelected()
  pendingCommand = read(0xC193)
end

local function onSoundSelectionReturn()
  if pendingCommand ~= 0x09 then
    pendingCommand = nil
    return
  end
  commandObserved = true
  captureStart = masterClock()
  expect(read(0xDD72) == 0x8B,
    "command $09 did not select active program $8B/$0B")
  expect(read(0xC194) == 0x23,
    "command $09 did not install priority window $23")
  print(string.format(
    "RIM_SOUND command=%02X program=%02X priority_frames=%02X stream=3EF2",
    pendingCommand, read(0xDD72), read(0xC194)))
  pendingCommand = nil
end

local function onApuWrite(address, value)
  if captureStart == nil then return end
  if address == 0xFF25 and lastRoute == value then return end
  if address == 0xFF25 then lastRoute = value end
  table.insert(events, {
    frame = totalFrames,
    cycles = masterClock() - captureStart,
    address = address,
    value = value
  })
end

local function onInputPolled()
  totalFrames = totalFrames + 1
  local input = {
    a = false, b = false, start = false, select = false,
    up = false, down = false, left = false, right = false
  }
  if not gameplayReached then
    if totalFrames % 30 == 1 then
      if rosterChooseCount >= 2 and not opponentMoved then
        input.right = true
        opponentMoved = true
      else
        input.start = true
      end
    end
  else
    gameplayFrames = gameplayFrames + 1
  end
  emu.setInput(input, 0)
end

local function onEndFrame()
  if stopping then return end
  if commandObserved and rimObserved and captureStart ~= nil and
     gameplayFrames >= 32 then
    stopping = true
    expect(#events > 0, "command $09 produced no APU writes")
    local wanted = {
      [0xFF20] = 0xEB, [0xFF21] = 0xF2,
      [0xFF22] = 0x5A, [0xFF23] = 0xBF
    }
    for address, value in pairs(wanted) do
      local found = false
      for _, event in ipairs(events) do
        if event.address == address and event.value == value then
          found = true
          break
        end
      end
      expect(found, string.format(
        "command $09 never wrote %04X=%02X", address, value))
    end
    print(string.format("RIM_AUDIO_CAPTURE events=%d", #events))
    for _, event in ipairs(events) do
      print(string.format(
        "APU frame=%d cycles=%d address=%04X value=%02X",
        event.frame, event.cycles, event.address, event.value))
    end
    print(failures == 0 and
      "TRACE PASSED: $1CED/$1D8C/$1F5F command-$09 rim audio" or
      string.format("TRACE FAILED: %d mismatch(es)", failures))
    emu.stop(failures == 0 and 0 or 3)
  elseif gameplayReached and gameplayFrames > 180 then
    stopping = true
    print("TRACE ERROR: rim-audio scenario timed out")
    emu.stop(2)
  elseif totalFrames > 3600 then
    stopping = true
    print("TRACE ERROR: One-on-One gameplay was not reached")
    emu.stop(2)
  end
end

emu.addMemoryCallback(onRosterChoose, emu.callbackType.exec,
  0x40F4, 0x40F4, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onPlayerUpdate, emu.callbackType.exec,
  0x702D, 0x702D, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onCourtDispatcher, emu.callbackType.exec,
  0x1CED, 0x1CED, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRimReturn, emu.callbackType.exec,
  0x1E5B, 0x1E5B, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onSoundSelected, emu.callbackType.exec,
  0x2F9E, 0x2F9E, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onSoundSelectionReturn, emu.callbackType.exec,
  0x2FAC, 0x2FAC, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onApuWrite, emu.callbackType.write,
  0xFF10, 0xFF26, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addEventCallback(onInputPolled, emu.eventType.inputPolled)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
