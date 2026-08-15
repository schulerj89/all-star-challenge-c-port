-- Headless Mesen trace for the cartridge's One-on-One shot launch/result path.
-- Replays the recovered make release and records the exact
-- player +$03/+18 selectors, $7C58 output vector, and $1CED's $1E0E score
-- dispatch. This diagnostic evidence does not modify ROM state.

local totalFrames = 0
local gameplayFrames = 0
local gameplayReached = false
local mem = emu.memType.gameboyDebug
local rosterChooseCount = 0
local opponentMoved = false
local initialGameplayState = nil
local reloadRequested = false
local scenario = 1
local releaseDelays = {37}
local launched = false
local scoreDispatch = false
local contacts = 0
local stopping = false
local failures = 0

local function read(address)
  return emu.read(address, mem, false)
end

local function word(lo)
  return read(lo) + read(lo + 1) * 256
end

local function expect(condition, message)
  if not condition then
    failures = failures + 1
    print("TRACE ERROR: " .. message)
  end
end

local function onPlayerUpdate()
  if reloadRequested then
    reloadRequested = false
    gameplayFrames = 0
    launched = false
    scoreDispatch = false
    contacts = 0
    emu.loadSavestate(initialGameplayState)
    return
  end
  if initialGameplayState == nil then
    initialGameplayState = emu.createSavestate()
  end
  gameplayReached = true
end

local function onRosterChoose()
  rosterChooseCount = rosterChooseCount + 1
end

local function onLaunch()
  launched = true
  print(string.format(
    "SHOT scenario=%d delay=%d p1_action=%02X record=%02X timer=%02X profile=%02X " ..
    "phase=%02X rng=%02X cooldown=%02X visual_y=%02X ground_y=%02X ball=%02X,%02X,%02X",
    scenario, releaseDelays[scenario], read(0xFF9D), read(0xFFA0),
    read(0xFFA1), read(0xFFB5), read(0xFFB0), read(0xFFFB), read(0xC17E),
    read(0xFFA2), read(0xFFB2),
    read(0xC0A3), read(0xC0A7), read(0xC0AB)))
  expect(read(0xFFA0) == 0x07 and read(0xFFB5) == 0x02 and
         read(0xFFA2) == 0x56 and read(0xFFB2) == 0x98 and
         read(0xC0AB) == 0x40,
         "$6A8C/$7F37 release selectors did not match the captured make")
end

local function onLaunchComplete()
  print(string.format(
    "VECTOR scenario=%d class=%02X shift=%02X vx=%04X vy=%04X vz=%04X",
    scenario, read(0xC17B), read(0xC17C), word(0xC0A0),
    word(0xC0A4), word(0xC0A8)))
  expect(read(0xC17B) == 0x03 and word(0xC0A0) == 0x0004 and
         word(0xC0A4) == 0xFF18 and word(0xC0A8) == 0x01C8,
         "$7C58 launch vector/table result did not match")
end

local function onContact()
  contacts = contacts + 1
  if launched and read(0xC0A7) <= 0x62 then
    print(string.format(
      "CONTACT scenario=%d n=%d xyz=%02X,%02X,%02X v=%04X,%04X,%04X phase=%02X",
      scenario, contacts, read(0xC0A3), read(0xC0A7), read(0xC0AB),
      word(0xC0A0), word(0xC0A4), word(0xC0A8), read(0xFFB0)))
  end
end

local function onScore()
  scoreDispatch = true
  print(string.format(
    "SCORE scenario=%d delay=%d branch=$1E0E xyz=%02X,%02X,%02X points_flag=%02X",
    scenario, releaseDelays[scenario], read(0xC0A3), read(0xC0A7),
    read(0xC0AB), read(0xFFD7)))
  expect(read(0xC0A3) == 0x54 and read(0xC0A7) == 0x5C and
         read(0xC0AB) == 0x38,
         "$1CED did not enter $1E0E at the traced score cell")
end

local function nextScenario()
  print(string.format("RESULT scenario=%d delay=%d launched=%s score=%s contacts=%d",
    scenario, releaseDelays[scenario], tostring(launched),
    tostring(scoreDispatch), contacts))
  expect(launched and scoreDispatch,
         "timed cartridge shot did not launch and score")
  scenario = scenario + 1
  if scenario > #releaseDelays then
    stopping = true
    print(failures == 0 and
      "TRACE PASSED: $6A8C/$7F37/$7C58/$1CED launched make" or
      string.format("TRACE FAILED: %d mismatch(es)", failures))
    emu.stop(failures == 0 and 0 or 3)
    return
  end
  reloadRequested = true
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
    input.a = gameplayFrames == 10 or
      gameplayFrames == 10 + releaseDelays[scenario]
  end
  emu.setInput(input, 0)
end

local function onEndFrame()
  if gameplayReached and gameplayFrames >= 125 and not reloadRequested then
    nextScenario()
  elseif not gameplayReached and totalFrames >= 3600 and not stopping then
    stopping = true
    print("TRACE ERROR: One-on-One gameplay was not reached")
    emu.stop(2)
  end
end

emu.addMemoryCallback(onPlayerUpdate, emu.callbackType.exec,
  0x702D, 0x702D, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRosterChoose, emu.callbackType.exec,
  0x40F4, 0x40F4, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onLaunch, emu.callbackType.exec,
  0x7C58, 0x7C58, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onLaunchComplete, emu.callbackType.exec,
  0x7F28, 0x7F28, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onContact, emu.callbackType.exec,
  0x1CED, 0x1CED, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onScore, emu.callbackType.exec,
  0x1E0E, 0x1E0E, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addEventCallback(onInputPolled, emu.eventType.inputPolled)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
