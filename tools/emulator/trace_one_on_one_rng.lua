-- Headless Mesen trace for the ROM's One-on-One random byte at $FFFB.
-- The cartridge copies a resident routine into $FF80..$FFFE; gameplay AI
-- consumes $FFFB directly.  This trace records the writer PC, every gameplay
-- frame value, and the values observed by reviewed random-consuming routines.

local totalFrames = 0
local gameplayFrames = 0
local gameplayReached = false
local rosterChooseCount = 0
local opponentMoved = false
local stopping = false
local mem = emu.memType.gameboyDebug
local writes = 0
local frameValues = {}
local failures = 0

local function read(address)
  return emu.read(address, mem, false)
end

local function cpuPc()
  local state = emu.getState()
  return state["cpu.pc"] or state["gb.cpu.pc"] or state["gameboy.cpu.pc"] or -1
end

local function onRandomWrite(address, value)
  if gameplayReached and gameplayFrames <= 32 then
    writes = writes + 1
    print(string.format(
      "RNG WRITE frame=%d pc=%04X address=%04X old=%02X%02X new_lo=%02X " ..
      "entropy=%02X/%02X input=%02X",
      gameplayFrames, cpuPc(), address, read(0xFFFC), read(address), value,
      read(0xC133), read(0xC0B6), read(0xFF8B)))
  end
end

local function consume(name)
  return function()
    if gameplayReached and gameplayFrames <= 32 then
      print(string.format("RNG CONSUME frame=%d pc=%s value=%02X",
        gameplayFrames, name, read(0xFFFB)))
    end
  end
end

local function onPlayerInputUpdate()
  gameplayReached = true
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
  if gameplayReached then
    local value = read(0xFFFB)
    local expected = 0x18
    for _ = 1, math.floor(gameplayFrames / 2) do
      expected = (expected * 9 + 0x2B) & 0xFF
    end
    if value ~= expected then
      failures = failures + 1
      print(string.format(
        "TRACE ERROR: frame %d expected $FFFB=%02X, observed %02X",
        gameplayFrames, expected, value))
    end
    frameValues[#frameValues + 1] = value
    print(string.format("RNG FRAME frame=%d seed=%02X%02X alternate=%02X%02X",
      gameplayFrames, read(0xFFFC), value, read(0xFFFE), read(0xFFFD)))
    if gameplayFrames >= 32 then
      stopping = true
      if writes == 0 then
        print("TRACE ERROR: no $FFFB gameplay writes were observed")
        emu.stop(3)
      elseif failures ~= 0 then
        print(string.format("TRACE FAILED: %d RNG mismatch(es)", failures))
        emu.stop(3)
      else
        print(string.format(
          "TRACE PASSED: $FFFB writer and 32 One-on-One frame values (%d writes)",
          writes))
        emu.stop(0)
      end
    end
  elseif totalFrames >= 3600 then
    stopping = true
    print("TRACE ERROR: One-on-One gameplay was not reached")
    emu.stop(2)
  end
end

emu.addMemoryCallback(onPlayerInputUpdate, emu.callbackType.exec,
  0x702D, 0x702D, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRosterChoose, emu.callbackType.exec,
  0x40F4, 0x40F4, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRandomWrite, emu.callbackType.write,
  0xFFFB, 0xFFFB, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(consume("$702D"), emu.callbackType.exec,
  0x702D, 0x702D, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(consume("$74BB"), emu.callbackType.exec,
  0x74BB, 0x74BB, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(consume("$75CD"), emu.callbackType.exec,
  0x75CD, 0x75CD, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addEventCallback(onInputPolled, emu.eventType.inputPolled)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
