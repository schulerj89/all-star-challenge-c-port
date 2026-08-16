-- Headless Accuracy Shootout trace. Follows menu mode $03 through its
-- settings and single-player selector into fixed $0E51 / bank-1 $6CA2.

local totalFrames = 0
local accuracyFrames = 0
local menuReached = false
local menuMoves = 0
local settingsSeen = false
local rosterEntryCount = 0
local rosterP1Count = 0
local accuracyReached = false
local positionCalls = 0
local launchCount = 0
local makeCount = 0
local attemptCount = 0
local scoreCount = 0
local currentCommand = 0
local apuBudget = 0
local lastSoundCommand = -1
local lastSoundFrame = -100
local approachActive = false
local readyFrames = 0
local exitSeen = false
local exitFrame = 0
local stopping = false
local command02Seen = false
local command02Apu = {}
local firstTargetX = -1
local firstTargetY = -1
local mem = emu.memType.gameboyDebug

local function read(address)
  return emu.read(address, mem, false)
end

local function write(address, value)
  emu.write(address, value, mem)
end

local function bytes(address, count)
  local values = {}
  for i = 0, count - 1 do
    values[#values + 1] = string.format("%02X", read(address + i))
  end
  return table.concat(values, "")
end

local function state(label)
  print(string.format(
    "ACCURACY_%s frame=%d mode=%02X players=%02X pos_index=%02X group=%02X target=%02X,%02X player=%02X,%02X timer=%02X%02X attempts=%02X%02X makes=%02X%02X flags=%02X/%02X/%02X",
    label, accuracyFrames, read(0xFF8F), read(0xFF91), read(0xFFE0),
    read(0xFFE1), read(0xFFDB), read(0xFFDC), read(0xFFA3), read(0xFFB2),
    read(0xC0B7), read(0xC0B6), read(0xC13A), read(0xC139),
    read(0xC138), read(0xC137), read(0xFFE8), read(0xFFF8), read(0xFFE2)))
end

local function dumpBg(label)
  for row = 0, 17 do
    print(string.format("ACCURACY_BG_%s row=%02d data=%s", label, row,
      bytes(0x9800 + row * 0x20, 20)))
  end
end

local function onMenuLoop()
  menuReached = true
end

local function onSettings()
  if read(0xFF8F) == 0x03 then
    settingsSeen = true
    print(string.format(
      "ACCURACY_SETTINGS_22EF mode=03 computer=%02X new=%02X time=%02X%02X",
      read(0xFF9A), read(0xFF9B), read(0xFF95), read(0xFF94)))
  end
end

local function onRosterEntry()
  if read(0xFF8F) == 0x03 then
    rosterEntryCount = rosterEntryCount + 1
    print(string.format("ACCURACY_ROSTER_4000 count=%d players=%02X",
      rosterEntryCount, read(0xFF91)))
  end
end

local function onRosterP1()
  if read(0xFF8F) == 0x03 then
    rosterP1Count = rosterP1Count + 1
    print(string.format("ACCURACY_ROSTER_4034 count=%d", rosterP1Count))
  end
end

local function onAccuracyEntry()
  accuracyReached = true
  accuracyFrames = 0
  state("ENTRY_0E51")
end

local function onAccuracyInit()
  state("INIT_6C9B")
end

local function onPositionBegin()
  positionCalls = positionCalls + 1
  approachActive = true
  readyFrames = 0
  if positionCalls == 1 then write(0xFFFB, 0x10) end
  state("POSITION_BEGIN_6CA2")
end

local function onPositionReady()
  state("POSITION_READY_6CE3")
  if positionCalls == 1 then
    firstTargetX = read(0xFFDB)
    firstTargetY = read(0xFFDC)
    dumpBg("GAMEPLAY")
  end
end

local function onApproach()
  state("APPROACH_7AFD")
end

local function onLaunch()
  launchCount = launchCount + 1
  approachActive = false
  state("LAUNCH_7C58")
end

local function onAttempt()
  attemptCount = attemptCount + 1
  state("ATTEMPT_0EE7")
end

local function onMake()
  makeCount = makeCount + 1
  state("MAKE_1E0E")
end

local function onScore()
  scoreCount = scoreCount + 1
  state("SCORE_0F1E")
end

local function onExit()
  exitSeen = true
  exitFrame = accuracyFrames
  state("EXIT_0FDE")
end

local function onSound()
  if accuracyReached then
    currentCommand = read(0xC193)
    if currentCommand == 0x02 then command02Seen = true end
    if currentCommand ~= lastSoundCommand or
        accuracyFrames - lastSoundFrame >= 30 then
      print(string.format(
        "ACCURACY_SOUND frame=%d command=%02X program=%02X priority=%02X",
        accuracyFrames, currentCommand, read(0xDD72), read(0xC194)))
      lastSoundCommand = currentCommand
      lastSoundFrame = accuracyFrames
      apuBudget = 12
    end
  end
end

local function onApuWrite(address, value)
  if accuracyReached and currentCommand ~= 0 and apuBudget > 0 and
      address >= 0xFF10 and address <= 0xFF26 then
    if currentCommand == 0x02 then command02Apu[address] = value end
    apuBudget = apuBudget - 1
    print(string.format(
      "ACCURACY_APU frame=%d command=%02X address=%04X value=%02X",
      accuracyFrames, currentCommand, address, value))
  end
end

local function onInputPolled()
  totalFrames = totalFrames + 1
  local input = {
    a = false, b = false, start = false, select = false,
    up = false, down = false, left = false, right = false
  }
  if not accuracyReached then
    if not menuReached and totalFrames % 30 == 1 then
      input.start = true
    elseif menuReached and totalFrames % 30 == 1 then
      if menuMoves < 3 then
        input.down = true
        menuMoves = menuMoves + 1
      else
        input.start = true
      end
    end
  else
    accuracyFrames = accuracyFrames + 1
    if approachActive then
      local targetX = read(0xFFDB)
      local targetY = read(0xFFDC)
      local centerX = (read(0xFFA3) + 8) % 256
      local groundY = read(0xFFB2)
      if centerX > targetX + 3 then input.left = true
      elseif centerX + 3 < targetX then input.right = true end
      if groundY > targetY + 3 then input.up = true
      elseif groundY + 3 < targetY then input.down = true end
      if not input.left and not input.right and not input.up and
          not input.down then
        readyFrames = readyFrames + 1
        input.a = readyFrames == 4 or readyFrames == 38 or
          (readyFrames > 38 and readyFrames % 40 == 0)
      end
    elseif launchCount > attemptCount and accuracyFrames % 30 == 0 then
      input.a = false
    end
    if accuracyFrames == 1050 then
      write(0xC0B6, 0)
      write(0xC0B7, 0)
    end
  end
  emu.setInput(input, 0)
end

local function onEndFrame()
  if exitSeen and accuracyFrames >= exitFrame + 245 and not stopping then
    stopping = true
    state("SUMMARY")
    print(string.format(
      "ACCURACY_SUMMARY settings=%s roster4000=%d roster4034=%d positions=%d launches=%d attempts=%d makes=%d scores=%d",
      tostring(settingsSeen), rosterEntryCount, rosterP1Count, positionCalls,
      launchCount, attemptCount, makeCount, scoreCount))
    local ok = settingsSeen and rosterEntryCount == 1 and rosterP1Count == 1 and
      read(0xFF91) == 1 and firstTargetX == 0x0C and firstTargetY == 0x94 and
      positionCalls >= 4 and launchCount >= 3 and attemptCount >= 3 and
      makeCount >= 1 and scoreCount >= 1 and command02Seen and
      command02Apu[0xFF10] == 0x88 and command02Apu[0xFF11] == 0x00 and
      command02Apu[0xFF12] == 0xFF and command02Apu[0xFF13] == 0x5B and
      command02Apu[0xFF14] == 0xBE and command02Apu[0xFF16] == 0x3F and
      command02Apu[0xFF17] == 0x6F and command02Apu[0xFF18] == 0x41 and
      command02Apu[0xFF19] == 0xBE
    if ok then
      print("TRACE PASSED: $4000/$4034 one-player Accuracy, $6CA2/$7AFD positions, $0EE7/$0F1E scoring, and $0FDE command-$02 APU")
      emu.stop(0)
    else
      print("TRACE ERROR: Accuracy assertions failed")
      emu.stop(3)
    end
  elseif totalFrames >= 8000 and not stopping then
    stopping = true
    print("TRACE ERROR: Accuracy Shootout was not reached")
    emu.stop(2)
  end
end

emu.addMemoryCallback(onMenuLoop, emu.callbackType.exec,
  0x03B9, 0x03B9, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onSettings, emu.callbackType.exec,
  0x22EF, 0x22EF, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRosterEntry, emu.callbackType.exec,
  0x4000, 0x4000, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRosterP1, emu.callbackType.exec,
  0x4034, 0x4034, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onAccuracyEntry, emu.callbackType.exec,
  0x0E51, 0x0E51, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onAccuracyInit, emu.callbackType.exec,
  0x6C9B, 0x6C9B, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onPositionBegin, emu.callbackType.exec,
  0x6CA2, 0x6CA2, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onPositionReady, emu.callbackType.exec,
  0x6CE3, 0x6CE3, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onApproach, emu.callbackType.exec,
  0x7AFD, 0x7AFD, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onLaunch, emu.callbackType.exec,
  0x7C58, 0x7C58, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onAttempt, emu.callbackType.exec,
  0x0EE7, 0x0EE7, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onMake, emu.callbackType.exec,
  0x1E0E, 0x1E0E, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onScore, emu.callbackType.exec,
  0x0F1E, 0x0F1E, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onExit, emu.callbackType.exec,
  0x0FDE, 0x0FDE, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onSound, emu.callbackType.exec,
  0x2F9E, 0x2F9E, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onApuWrite, emu.callbackType.write,
  0xFF10, 0xFF26, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addEventCallback(onInputPolled, emu.eventType.inputPolled)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
