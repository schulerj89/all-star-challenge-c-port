-- Headless Mesen assertions for the One-on-One art and ball-OAM path.
-- The ROM initializes ball/shadow graphics through $1FFA/$20BA, court art
-- through $2243->$04B1/$050F, then composes the ball through $6945/$69F5.

local totalFrames = 0
local gameplayReached = false
local rosterChooseCount = 0
local opponentMoved = false
local stopping = false
local failures = 0
local scenario = 1
local assetsChecked = false
local pendingScenario = 0
local oamDone = false
local ballOam = 0xC060
local shadowOam = 0xC068
local mem = emu.memType.gameboyDebug

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

local function signature(startAddress, length)
  local sum = 0
  local weighted = 0
  local xorValue = 0
  for i = 0, length - 1 do
    local value = read(startAddress + i)
    sum = (sum + value) & 0xFFFF
    weighted = (weighted + ((i + 1) * value)) & 0xFFFF
    xorValue = xorValue ~ value
  end
  return sum, weighted, xorValue
end

local function checkInitializedAssets()
  if assetsChecked then return end
  assetsChecked = true
  local sum, weighted, xorValue = signature(0x8240, 1344)
  expect(sum == 23349 and weighted == 53348 and xorValue == 219,
    "$1FFA/$20BA ball/shadow VRAM expansion did not match the ROM stream")
  print(string.format("ASSET ball_vram len=1344 sum=%04X weighted=%04X xor=%02X",
    sum, weighted, xorValue))

  sum, weighted, xorValue = signature(0x9000, 1376)
  expect(sum == 42313 and weighted == 39918 and xorValue == 63,
    "$2243/$050F court tile expansion did not match the ROM stream")
  print(string.format("ASSET court_tiles len=1376 sum=%04X weighted=%04X xor=%02X",
    sum, weighted, xorValue))

  sum, weighted, xorValue = signature(0x9800, 640)
  expect(sum == 7323 and weighted == 9648 and xorValue == 113,
    "$04B1/$050F court tilemap expansion did not match the ROM stream")
  print(string.format("ASSET court_map len=640 sum=%04X weighted=%04X xor=%02X",
    sum, weighted, xorValue))
  local bytes = {}
  for i = 0, 31 do bytes[#bytes + 1] = string.format("%02X", read(0x9800 + i)) end
  print("ASSET court_map_head " .. table.concat(bytes, " "))
end

local function checkBallOam(checkedScenario)
  local expectedBallY = checkedScenario == 1 and 0x69 or
    (checkedScenario == 2 and 0x68 or 0x51)
  local expectedShadowLeft = checkedScenario == 1 and 0x5C or
    (checkedScenario == 2 and 0x5E or 0x48)
  local expectedShadowRight = checkedScenario == 1 and 0x60 or
    (checkedScenario == 2 and 0x60 or 0x4A)
  local expectedZ = checkedScenario == 1 and 0x07 or
    (checkedScenario == 2 and 0x08 or 0x1F)

  expect(read(0xFFF3) == 3 and read(0xFFF2) == 3,
    "$6945 did not select phase ball_x&7")
  expect(read(ballOam) == expectedBallY and read(ballOam + 1) == 0x40 and
         read(ballOam + 2) == 0x2E and read(ballOam + 4) == expectedBallY and
         read(ballOam + 5) == 0x48 and read(ballOam + 6) == 0x30,
    "$69F5 ball OAM pair did not match phase three")
  expect(read(shadowOam) == 0x70 and read(shadowOam + 1) == 0x40 and
         read(shadowOam + 2) == expectedShadowLeft and
         read(shadowOam + 4) == 0x70 and read(shadowOam + 5) == 0x48 and
         read(shadowOam + 6) == expectedShadowRight,
    "$6945 shadow tier/OAM pair did not match ball height")
  print(string.format(
    "OAM phase=3 z=%02X ptr=%04X/%04X ball=%02X/%02X shadow=%02X/%02X",
    expectedZ, ballOam, shadowOam,
    read(ballOam + 2), read(ballOam + 6),
    read(shadowOam + 2), read(shadowOam + 6)))
end

local function onBallComposer()
  if not assetsChecked then return end
  gameplayReached = true
  -- Mesen's execution callback at the RET boundary can precede the final
  -- in-block OAM writes, so validate the completed pair at the next entry.
  if pendingScenario ~= 0 then
    checkBallOam(pendingScenario)
    if pendingScenario == 3 then oamDone = true end
    pendingScenario = 0
  end
  if scenario > 3 then return end

  ballOam = read(0xC148) | (read(0xC149) << 8)
  shadowOam = read(0xC14A) | (read(0xC14B) << 8)
  write(0xFFCF, 0)
  write(0xC0A3, 0x43)
  write(0xC0A7, 0x70)
  write(0xC0AB, scenario == 1 and 0x07 or
    (scenario == 2 and 0x08 or 0x1F))
  pendingScenario = scenario
  scenario = scenario + 1
end

local function onCourtLoadReturn()
  if read(0xC26F) == 1 then checkInitializedAssets() end
end

local function onInputPolled()
  totalFrames = totalFrames + 1
  local input = {
    a = false, b = false, start = false, select = false,
    up = false, down = false, left = false, right = false
  }
  if not gameplayReached and totalFrames % 30 == 1 then
    if rosterChooseCount >= 2 and not opponentMoved then
      input.right = true
      opponentMoved = true
    else
      input.start = true
    end
  end
  emu.setInput(input, 0)
end

local function onRosterChoose()
  rosterChooseCount = rosterChooseCount + 1
end

local function onEndFrame()
  if stopping then return end
  if oamDone and assetsChecked then
    stopping = true
    print(failures == 0 and
      "TRACE PASSED: One-on-One asset VRAM and $6945/$69F5 ball OAM" or
      string.format("TRACE FAILED: %d mismatch(es)", failures))
    emu.stop(failures == 0 and 0 or 3)
  elseif totalFrames >= 3600 then
    stopping = true
    print("TRACE ERROR: One-on-One gameplay was not reached")
    emu.stop(2)
  end
end

emu.addMemoryCallback(onBallComposer, emu.callbackType.exec,
  0x6945, 0x6945, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onCourtLoadReturn, emu.callbackType.exec,
  0x04E2, 0x04E2, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRosterChoose, emu.callbackType.exec,
  0x40F4, 0x40F4, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addEventCallback(onInputPolled, emu.eventType.inputPolled)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
