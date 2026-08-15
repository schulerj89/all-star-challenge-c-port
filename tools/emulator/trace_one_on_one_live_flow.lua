-- Headless Mesen proof for the ROM's live One-on-One movement/shot flow.
-- Holds Right through a complete dribble/run loop, presses A while Right stays
-- held, and proves that $702D->$714D->$6A8C keeps applying $6B72 movement
-- during the phase-zero shot gather before the second A releases the ball.

local totalFrames = 0
local gameplayFrames = 0
local gameplayReached = false
local rosterChooseCount = 0
local opponentMoved = false
local failures = 0
local stopping = false
local mem = emu.memType.gameboyDebug
local P1 = 0xFF9D
local runRecords = {}
local runDisplays = {}
local gatherStartX = nil
local gatherEndX = nil
local gatherRecords = {}
local launched = false

local function read(address)
  return emu.read(address, mem, false)
end

local function write(address, value)
  emu.write(address, value, mem)
end

local function expect(condition, message)
  if not condition then
    failures = failures + 1
    print("TRACE ERROR: " .. message)
  end
end

local function countKeys(values)
  local count = 0
  for _ in pairs(values) do count = count + 1 end
  return count
end

local function onRosterChoose()
  rosterChooseCount = rosterChooseCount + 1
end

local function onPlayerUpdate()
  gameplayReached = true
  -- Keep the defender away so the trace measures animation-record movement,
  -- not the separately proven $6E3C contact rejection path.
  write(0xFFBC, 0x20)
  write(0xFFCB, 0x70)
end

local function onLaunch()
  launched = true
  gatherEndX = read(P1 + 6)
  print(string.format(
    "LIVE_RELEASE frame=%d action=%02X record=%02X display=%02X input=%02X x=%02X z=%02X",
    gameplayFrames, read(P1), read(P1 + 3), read(P1 + 1),
    read(P1 + 7), read(P1 + 6), read(0xC0AB)))
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
    if gameplayFrames >= 5 and gameplayFrames <= 120 then
      input.right = true
    end
    input.a = gameplayFrames == 70 or gameplayFrames == 112
  end
  emu.setInput(input, 0)
end

local function onEndFrame()
  if gameplayReached then
    local action = read(P1)
    local record = read(P1 + 3)
    local display = read(P1 + 1)
    local x = read(P1 + 6)
    if gameplayFrames >= 5 and gameplayFrames < 70 and action == 0x10 then
      runRecords[record] = true
      runDisplays[display] = true
    end
    if gameplayFrames == 70 then gatherStartX = x end
    if gameplayFrames >= 70 and gameplayFrames < 112 and
       (action == 0x0A or action == 0x12) then
      gatherRecords[record] = true
      if gameplayFrames % 6 == 0 or gameplayFrames == 70 then
        print(string.format(
          "LIVE_GATHER frame=%d action=%02X record=%02X timer=%02X display=%02X input=%02X x=%02X",
          gameplayFrames, action, record, read(P1 + 4), display,
          read(P1 + 7), x))
      end
    end
    if gameplayFrames >= 125 and not stopping then
      stopping = true
      expect(countKeys(runRecords) >= 8,
        "$6A8C did not traverse the complete eight-record run cycle")
      expect(countKeys(runDisplays) >= 4,
        "$6A8C run cycle did not expose all four display frames")
      expect(gatherStartX ~= nil and gatherEndX ~= nil and
             gatherEndX > gatherStartX,
        "$714D shot gather did not preserve Right movement through $6B72")
      expect(countKeys(gatherRecords) >= 5,
        "shot gather did not continue through multiple $6A8C records")
      expect(launched, "second A did not reach $7C58")
      print(string.format(
        "LIVE_SUMMARY run_records=%d run_displays=%d gather_records=%d gather_x=%02X->%02X",
        countKeys(runRecords), countKeys(runDisplays),
        countKeys(gatherRecords), gatherStartX or 0, gatherEndX or 0))
      print(failures == 0 and
        "TRACE PASSED: $702D/$714D/$6A8C shot-gather movement and full run cycle" or
        string.format("TRACE FAILED: %d mismatch(es)", failures))
      emu.stop(failures == 0 and 0 or 3)
    end
  elseif totalFrames >= 3600 and not stopping then
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
emu.addEventCallback(onInputPolled, emu.eventType.inputPolled)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
