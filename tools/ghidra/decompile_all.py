# Ghidra Headless Script: decompile_all.py
# Exports reviewed Game Boy functions to a single preliminary C listing.
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
from ghidra.app.cmd.disassemble import DisassembleCommand
import os

print("[Ghidra] Initializing Game Boy disassembler & decompiler...")

addr_factory = currentProgram.getAddressFactory()
space = addr_factory.getDefaultAddressSpace()

# Standard entry points and interrupt vectors for Game Boy
entry_offsets = [
    0x0000, 0x0008, 0x0010, 0x0018, 0x0020, 0x0028, 0x0030, 0x0038, # RST vectors
    0x0040, # VBlank
    0x0048, # LCD STAT
    0x0050, # Timer
    0x0058, # Serial
    0x0060, # Joypad
    0x0100, # Boot / Entrypoint
    0x0150  # Main initialization Jump
]

monitor = ConsoleTaskMonitor()

# Disassemble from known entrypoints
for off in entry_offsets:
    addr = space.getAddress(off)
    cmd = DisassembleCommand(addr, None, True)
    cmd.applyTo(currentProgram, monitor)

listing = currentProgram.getListing()

# Ensure the standard cartridge entry points and vectors exist.
createFunction(space.getAddress(0x0100), "entry_boot")
createFunction(space.getAddress(0x0150), "init_game")

for off in entry_offsets:
    a = space.getAddress(off)
    if listing.getInstructionAt(a) is not None:
        createFunction(a, "vec_%04x" % off)

# Initialize Decompiler
decompiler = DecompInterface()
decompiler.openProgram(currentProgram)

decomp_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "decomp")
if not os.path.exists(decomp_dir):
    try:
        os.makedirs(decomp_dir)
    except:
        pass

out_file = os.path.join(decomp_dir, "ghidra_decompiled.c")
f = open(out_file, "w")
f.write("/* ========================================================================= */\n")
f.write("/* Ghidra Decompiled Source for NBA All-Star Challenge (Game Boy)          */\n")
f.write("/* Architecture: Sharp SM83 (LR35902)                                       */\n")
f.write("/* ========================================================================= */\n\n")

fm = currentProgram.getFunctionManager()
funcs = fm.getFunctions(True)

count = 0
for func in funcs:
    entry = func.getEntryPoint().toString()
    name = func.getName()
    res = decompiler.decompileFunction(func, 30, monitor)
    if res and res.decompileCompleted():
        decomp_func = res.getDecompiledFunction()
        if decomp_func:
            c_code = decomp_func.getC()
            f.write("/* ------------------------------------------------------------------------- */\n")
            f.write("/* Function: " + name + " @ " + entry + " */\n")
            f.write("/* ------------------------------------------------------------------------- */\n")
            f.write(c_code + "\n\n")
            count += 1

f.close()
print("[Ghidra] Reviewed decompilation export wrote %d functions to %s" % (count, out_file))
