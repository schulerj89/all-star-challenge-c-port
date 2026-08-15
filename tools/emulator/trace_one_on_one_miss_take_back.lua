-- Headless Mesen proof for a rim miss, outer-boundary dead ball, defensive
-- recovery, $FFD1 take-back lock, and CPU ownership of that same state.

local totalFrames = 0
local gameplayFrames = 0
local gameplayReached = false
local rosterChooseCount = 0
local opponentMoved = false
local mem = emu.memType.gameboyDebug
local failures = 0
local stopping = false
local stage = 0
local rimObserved = false
local boundaryObserved = false
local recoveryArmed = false
local recoveryObserved = false
local insideObserved = false
local outsideObserved = false
local cpuRouteObserved = false
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

local function setWord(address, value)
  write(address, value & 0xFF)
  write(address + 1, (value >> 8) & 0xFF)
end

local function placePlayer(base, action, rawX, visualY, groundY, noBall)
  write(base + 0x00, action)
  write(base + 0x03, 1)
  write(base + 0x04, 6)
  write(base + 0x05, visualY)
  write(base + 0x06, rawX)
  write(base + 0x09, noBall and 1 or 0)
  write(base + 0x15, groundY)
end

local function onPlayerUpdate()
  gameplayReached = true
  if boundaryObserved and not recoveryArmed then
    -- $077D reference is raw X+8 and ground Y-2. Put only P2 on the ball.
    placePlayer(P1, 0x06, 0x20, 0x60, 0x88, true)
    placePlayer(P2, 0x06, 0x50, 0x70, 0x98, true)
    write(0xFFCF, 0)
    write(0xFFD0, 1)
    write(0xFFE2, 0)
    write(0xFFE7, 0)
    write(0xFFF8, 0)
    write(0xC12D, 0)
    setWord(0xC0A2, 0x5800)
    setWord(0xC0A6, 0x9600)
    setWord(0xC0AA, 0x0000)
    recoveryArmed = true
    print("RECOVERY_ARMED player=02 ball=58,96,00 prior_owner=01")
  end
end

local function onCourtDispatcher()
  if not gameplayReached then return end
  if stage == 0 then
    -- Exact $1D8C left-rim miss cell: X=$53,Y=$5E,Z=$37.
    write(0xFFCF, 0)
    write(0xFFD0, 1)
    write(0xFFB0, 0)
    write(0xFFC9, 0)
    setWord(0xC0A0, 0x0000)
    setWord(0xC0A2, 0x5300)
    setWord(0xC0A4, 0x0000)
    setWord(0xC0A6, 0x5E00)
    setWord(0xC0A8, 0xFFB0)
    setWord(0xC0AA, 0x3700)
    write(0xC17E, 0)
    stage = 1
  elseif stage == 2 then
    -- Exact outer limit: X=$A0 invokes $1F4D and zeros planar words.
    setWord(0xC0A0, 0x0123)
    setWord(0xC0A2, 0xA000)
    setWord(0xC0A4, 0xFEDC)
    setWord(0xC0A6, 0x9000)
    setWord(0xC0AA, 0x2000)
    stage = 3
  end
end

local function onRimReturn()
  if stage ~= 1 then return end
  rimObserved = true
  expect(read(0xC0A0) == 0x46 and read(0xC0A1) == 0x00,
    "$1D8C X=$53 rim miss did not install raw VX=$0046")
  expect(read(0xC17E) == 0x08,
    "$1F5F rim contact did not install the eight-frame cooldown")
  print(string.format("RIM_MISS x=%02X y=%02X z=%02X vx=%02X%02X cooldown=%02X",
    read(0xC0A3), read(0xC0A7), read(0xC0AB),
    read(0xC0A1), read(0xC0A0), read(0xC17E)))
  stage = 2
end

local function onBoundaryReturn()
  if stage ~= 3 then return end
  boundaryObserved = true
  expect(read(0xC0A0) == 0 and read(0xC0A1) == 0 and
         read(0xC0A4) == 0 and read(0xC0A5) == 0,
    "$1F4D did not zero both planar 8.8 velocity words")
  print(string.format("OUT_OF_BOUNDS x=%02X y=%02X vx=%02X%02X vy=%02X%02X",
    read(0xC0A3), read(0xC0A7), read(0xC0A1), read(0xC0A0),
    read(0xC0A5), read(0xC0A4)))
  stage = 4
end

local function onRecoveryCommitted()
  if not recoveryArmed or recoveryObserved then return end
  recoveryObserved = true
  expect(read(0xFFCF) == 2 and read(0xFFD1) == 1,
    "$2B88 did not award P2 and set changed-possession $FFD1")
  print(string.format("DEFENSIVE_RECOVERY owner=%02X take_back=%02X cooldown=%02X",
    read(0xFFCF), read(0xFFD1), read(0xC12D)))
end

local function onCpuRoute()
  if recoveryObserved and read(0xFFCF) == 2 and read(0xFFD1) == 1 then
    cpuRouteObserved = true
    print(string.format("CPU_TAKE_BACK_ROUTE owner=%02X take_back=%02X ball_x=%02X",
      read(0xFFCF), read(0xFFD1), read(0xC0A3)))
  end
end

local function onTakeBackCheck()
  if not recoveryObserved then return end
  if not insideObserved then
    write(0xFFD1, 1)
    write(0xC0A3, 0x54)
    write(0xC0A7, 0x70)
    stage = 5
  elseif not outsideObserved then
    write(0xFFD1, 1)
    write(0xC0A3, 0x20)
    write(0xC0A7, 0x70)
    stage = 6
  end
end

local function onTakeBackReturn()
  if stage == 5 then
    insideObserved = true
    expect(read(0xFFD1) == 1,
      "$78E9 cleared take-back while ball remained inside $796C")
    print(string.format("TAKE_BACK_INSIDE ball=%02X,%02X flag=%02X",
      read(0xC0A3), read(0xC0A7), read(0xFFD1)))
  elseif stage == 6 then
    outsideObserved = true
    expect(read(0xFFD1) == 0,
      "$78E9 did not clear take-back outside $796C")
    print(string.format("TAKE_BACK_CLEARED ball=%02X,%02X flag=%02X",
      read(0xC0A3), read(0xC0A7), read(0xFFD1)))
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
  if stopping then return end
  if rimObserved and boundaryObserved and recoveryObserved and
     insideObserved and outsideObserved and cpuRouteObserved then
    stopping = true
    print(failures == 0 and
      "TRACE PASSED: rim miss, $1F4D boundary, $2B88 recovery, CPU, and $78E9 take-back" or
      string.format("TRACE FAILED: %d mismatch(es)", failures))
    emu.stop(failures == 0 and 0 or 3)
  elseif gameplayReached and gameplayFrames > 360 then
    stopping = true
    print("TRACE ERROR: miss/take-back scenario timed out")
    emu.stop(2)
  elseif totalFrames > 3600 then
    stopping = true
    print("TRACE ERROR: One-on-One gameplay was not reached")
    emu.stop(2)
  end
end

emu.addMemoryCallback(onPlayerUpdate, emu.callbackType.exec,
  0x702D, 0x702D, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onCourtDispatcher, emu.callbackType.exec,
  0x1CED, 0x1CED, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRimReturn, emu.callbackType.exec,
  0x1E5B, 0x1E5B, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onBoundaryReturn, emu.callbackType.exec,
  0x1D06, 0x1D06, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRecoveryCommitted, emu.callbackType.exec,
  0x2BB5, 0x2BB5, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onCpuRoute, emu.callbackType.exec,
  0x72EA, 0x72EA, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onTakeBackCheck, emu.callbackType.exec,
  0x78E9, 0x78E9, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onTakeBackReturn, emu.callbackType.exec,
  0x7909, 0x7909, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRosterChoose, emu.callbackType.exec,
  0x40F4, 0x40F4, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addEventCallback(onInputPolled, emu.eventType.inputPolled)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
