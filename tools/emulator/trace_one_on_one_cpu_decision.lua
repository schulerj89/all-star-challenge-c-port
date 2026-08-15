-- Headless Mesen proof for bank-1 $7170 CPU offense.  The script awards the
-- live ball to the CPU, then records the target-selection, arrival, gather,
-- record-gated $756C decision, and eventual $7C58 release path.

local totalFrames = 0
local gameplayFrames = 0
local gameplayReached = false
local rosterChooseCount = 0
local opponentMoved = false
local possessionInjected = false
local stopping = false
local failures = 0
local mem = emu.memType.gameboyDebug
local P1 = 0xFF9D
local P2 = 0xFFB6
local targetSelections = 0
local arrivals = 0
local shotChecks = 0
local gatherFrame = nil
local launchFrame = nil
local lastState = ""

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

local function onRosterChoose()
  rosterChooseCount = rosterChooseCount + 1
end

local function onPlayerUpdate()
  gameplayReached = true
  if possessionInjected then
    write(P1 + 6, 0x90)
    write(P1 + 0x15, 0x60)
  end
  if gameplayFrames >= 5 and not possessionInjected then
    possessionInjected = true
    write(0xFFCF, 2)
    write(0xFFD0, 2)
    -- Keep the defender away from the CPU route.
    write(P1 + 6, 0x18)
    write(P1 + 0x15, 0x68)
    print(string.format(
      "CPU_POSSESSION frame=%d actor=%02X owner=%02X p2=%02X,%02X",
      gameplayFrames, read(0xC127), read(0xFFCF),
      read(P2 + 6), read(P2 + 0x15)))
  end
end

local function onTargetSelect()
  if not possessionInjected then return end
  write(0xFFFC, 0xCA)
  targetSelections = targetSelections + 1
  print(string.format(
    "CPU_TARGET_ENTER frame=%d rng=%02X ball_x=%02X stage=%02X/%02X",
    gameplayFrames, read(0xFFFC), read(0xC0A3),
    read(0xC0F7), read(0xC0F8)))
end

local function onRouteSelect()
  if not possessionInjected then return end
  write(0xFFFD, 0x00)
  write(0xFFFE, 0x98)
  print(string.format(
    "CPU_ROUTE_SELECT frame=%d roster=%02X entropy=%02X/%02X",
    gameplayFrames, read(P2 + 0x0F), read(0xFFFD), read(0xFFFE)))
end

local function onTargetReady()
  if not possessionInjected then return end
  print(string.format(
    "CPU_TARGET_READY frame=%d target=%02X,%02X stage=%02X/%02X",
    gameplayFrames, read(0xC102), read(0xC101),
    read(0xC0F7), read(0xC0F8)))
end

local function onArrival()
  if not possessionInjected then return end
  arrivals = arrivals + 1
  print(string.format(
    "CPU_ARRIVE frame=%d p2=%02X,%02X target=%02X,%02X route=%02X action=%02X record=%02X",
    gameplayFrames, read(P2 + 6), read(P2 + 0x15),
    read(0xC102), read(0xC101), read(0xC0FA),
    read(P2), read(P2 + 3)))
end

local function onArmGather()
  if not possessionInjected then return end
  if gatherFrame == nil then gatherFrame = gameplayFrames end
  print(string.format(
    "CPU_GATHER_INPUT frame=%d stored_rng=%02X action=%02X record=%02X",
    gameplayFrames, read(0xFFFB), read(P2), read(P2 + 3)))
end

local function onShotCheck()
  if not possessionInjected then return end
  shotChecks = shotChecks + 1
  if shotChecks <= 20 then
    print(string.format(
      "CPU_SHOT_CHECK frame=%d profile=%02X stored_rng=%02X live_rng=%02X action=%02X record=%02X p2=%02X,%02X",
      gameplayFrames, read(P2 + 0x18), read(0xC141), read(0xFFFB),
      read(P2), read(P2 + 3), read(P2 + 6), read(P2 + 0x15)))
  end
end

local function onLaunch()
  if read(0xFFCF) ~= 2 and read(0xC127) ~= 2 then return end
  launchFrame = gameplayFrames
  print(string.format(
    "CPU_RELEASE frame=%d gather_delay=%d action=%02X record=%02X stored_rng=%02X",
    gameplayFrames, gatherFrame and gameplayFrames - gatherFrame or -1,
    read(P2), read(P2 + 3), read(0xC141)))
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
  if possessionInjected then
    local state = string.format("%02X/%02X/%02X/%02X/%02X/%02X",
      read(0xC0F7), read(0xC0F8), read(0xC0FA),
      read(0xFFD2), read(0xFFD3), read(P2))
    if state ~= lastState then
      lastState = state
      print(string.format(
        "CPU_STATE frame=%d route=%s record=%02X p2=%02X,%02X target=%02X,%02X",
        gameplayFrames, state, read(P2 + 3), read(P2 + 6),
        read(P2 + 0x15), read(0xC102), read(0xC101)))
    end
  end
  if launchFrame ~= nil and gameplayFrames >= launchFrame + 2 and
     not stopping then
    stopping = true
    expect(targetSelections >= 1, "$72EA did not select an offense target")
    expect(arrivals >= 1, "$74BB did not complete a target arrival")
    expect(gatherFrame ~= nil, "$755D did not arm CPU A/gather input")
    expect(shotChecks >= 2,
      "$756C did not wait across animation-record shot checks")
    expect(launchFrame > gatherFrame,
      "CPU released immediately instead of waiting in shot gather")
    print(string.format(
      "CPU_SUMMARY targets=%d arrivals=%d shot_checks=%d gather_to_release=%d",
      targetSelections, arrivals, shotChecks, launchFrame - gatherFrame))
    print(failures == 0 and
      "TRACE PASSED: $7170/$72EA/$74BB/$755D/$756C CPU drive-gather-release" or
      string.format("TRACE FAILED: %d mismatch(es)", failures))
    emu.stop(failures == 0 and 0 or 3)
  elseif gameplayReached and gameplayFrames >= 900 and not stopping then
    stopping = true
    print("TRACE ERROR: CPU decision path did not release within 900 frames")
    emu.stop(2)
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
emu.addMemoryCallback(onTargetSelect, emu.callbackType.exec,
  0x72EA, 0x72EA, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onTargetReady, emu.callbackType.exec,
  0x749E, 0x749E, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRouteSelect, emu.callbackType.exec,
  0x732C, 0x732C, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onArrival, emu.callbackType.exec,
  0x751D, 0x751D, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onArmGather, emu.callbackType.exec,
  0x755D, 0x755D, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onShotCheck, emu.callbackType.exec,
  0x756C, 0x756C, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onLaunch, emu.callbackType.exec,
  0x7C58, 0x7C58, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addEventCallback(onInputPolled, emu.eventType.inputPolled)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
