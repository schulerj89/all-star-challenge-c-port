-- Deterministic Mesen proof for the remaining mode-0 $7170 CPU branches.
-- Existing trace_one_on_one_cpu_decision.lua covers offense through release;
-- this fixture injects loose-ball chase, steal, contest, $75CD contact hold,
-- and $74BB direction hysteresis at the $7170 entry boundary.

local totalFrames = 0
local gameplayFrames = 0
local gameplayReached = false
local rosterChooseCount = 0
local opponentMoved = false
local scenario = 1
local armedFrame = -1
local failures = 0
local stopping = false
local mem = emu.memType.gameboyDebug
local P1 = 0xFF9D
local P2 = 0xFFB6

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

local function clearController()
  write(0xFF8F, 0)
  write(0xFF90, 1)
  write(0xFFE2, 0)
  write(0xFFF8, 0)
  write(0xC12C, 0)
  write(0xC127, 2)
  write(0xFF97, 3)
  write(0xC0F7, 0)
  write(0xC0F8, 0)
  write(0xC0F9, 0)
  write(0xC0FA, 0)
  write(0xC0FB, 0)
  write(0xC0FC, 0)
  write(0xC0FD, 0)
  write(0xC0FF, 0)
  write(0xC100, 0)
  write(0xC103, 1)
  write(0xC106, 0)
  write(0xC144, 1)
  write(0xC16B, 0)
  write(P1, 0)
  write(P2, 0)
end

local function place(base, centerX, groundY, direction)
  write(base + 6, centerX - 8)
  write(base + 0x15, groundY)
  write(base + 0x10, direction or 1)
end

local function onController()
  gameplayReached = true
  if armedFrame == gameplayFrames then return end
  armedFrame = gameplayFrames
  clearController()
  if scenario == 1 then
    -- Loose-ball chase should emit right/down immediately at skill 3.
    write(0xFFCF, 0)
    place(P2, 0x40, 0x70, 1)
    write(0xC0A3, 0x64)
    write(0xC0A7, 0x78)
    write(0xC0AB, 0)
  elseif scenario == 2 then
    -- Strict $077D contact plus $45 < $46 emits new B at skill 3.
    write(0xFFCF, 1)
    place(P1, 0x50, 0x76, 2)
    place(P2, 0x50, 0x78, 1)
    write(0xC0A3, 0x50)
    write(0xC0A7, 0x76)
    write(0xC0AB, 0)
    write(0xFFFB, 0x45)
  elseif scenario == 3 then
    -- Initial opponent flight inside margin $0E emits new A/contest.
    write(0xFFCF, 1)
    write(0xFFD0, 1)
    write(0xFFF8, 1)
    place(P1, 0x20, 0x78, 2)
    place(P2, 0x54, 0x60, 1)
    write(0xC0A3, 0x20)
    write(0xC0A7, 0x78)
    write(0xFFFB, 0x80)
  elseif scenario == 4 then
    -- Qualified body block stores the exact CPU point and a ten-count hold.
    write(0xFFCF, 1)
    place(P1, 0x20, 0x78, 2)
    place(P2, 0x50, 0x70, 1)
    write(0xC0A3, 0x20)
    write(0xC0A7, 0x78)
    write(0xC16B, 1)
    write(0xFFFB, 0xFF)
  elseif scenario == 5 then
    -- A new rightward target must retain old left for two hysteresis calls.
    write(0xFFCF, 0)
    place(P2, 0x40, 0x70, 1)
    write(0xC0A3, 0x64)
    write(0xC0A7, 0x70)
    write(0xC0FE, 0x20)
    write(0xC103, 3)
    write(0xC144, 3)
  end
end

local function onRosterChoose()
  rosterChooseCount = rosterChooseCount + 1
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
  if stopping or not gameplayReached then
    if not gameplayReached and totalFrames >= 3600 and not stopping then
      stopping = true
      print("TRACE ERROR: One-on-One gameplay was not reached")
      emu.stop(2)
    end
    return
  end
  if armedFrame ~= gameplayFrames then return end
  print(string.format(
    "CPU_BRANCH scenario=%d new=%02X held=%02X arrived=%02X " ..
    "hold=%02X saved=%02X,%02X hysteresis=%02X accepted=%02X",
    scenario, read(0xFFD2), read(0xFFD3), read(0xC0FD),
    read(0xC0FC), read(0xC105), read(0xC104),
    read(0xC103), read(0xC0FE)))
  if scenario == 1 then
    expect((read(0xFFD3) & 0x90) == 0x90,
      "$7476 loose-ball chase did not emit right/down")
  elseif scenario == 2 then
    expect((read(0xFFD2) & 2) ~= 0,
      "$71B3 contact did not emit new B")
  elseif scenario == 3 then
    expect((read(0xFFD2) & 1) ~= 0,
      "$71EE initial-flight contest did not emit new A")
  elseif scenario == 4 then
    expect(read(0xC0FC) == 0x0A and read(0xC105) == 0x50 and
      read(0xC104) == 0x70,
      "$75CD did not save the point and load ten-count hold")
  elseif scenario == 5 then
    expect(read(0xFFD3) == 0x20 and read(0xC103) == 2 and
      read(0xC0FE) == 0x20,
      "$74BB did not retain prior direction during hysteresis")
  end
  scenario = scenario + 1
  armedFrame = -1
  if scenario > 5 then
    stopping = true
    print(failures == 0 and
      "TRACE PASSED: $7170 defense/chase/steal/contest/contact/hysteresis" or
      string.format("TRACE FAILED: %d mismatch(es)", failures))
    emu.stop(failures == 0 and 0 or 3)
  end
end

emu.addMemoryCallback(onController, emu.callbackType.exec,
  0x7170, 0x7170, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRosterChoose, emu.callbackType.exec,
  0x40F4, 0x40F4, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addEventCallback(onInputPolled, emu.eventType.inputPolled)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
