# Ghidra Headless Script: decompile_all.py
# Exports full C pseudocode for all functions in the Game Boy binary
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
import os

program_name = currentProgram.getName()
print("[Ghidra] Starting full decompilation of: " + program_name)

decompiler = DecompInterface()
decompiler.openProgram(currentProgram)
monitor = ConsoleTaskMonitor()

project_dir = getProjectRootFolder().getProjectLocator().getLocation()
decomp_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "decomp")
if not os.path.exists(decomp_dir):
    try:
        os.makedirs(decomp_dir)
    except:
        pass

out_file = os.path.join(decomp_dir, "ghidra_decompiled.c")
f = open(out_file, "w")
f.write("/* Ghidra Decompilation: " + program_name + " */\n")
f.write("/* Target Architecture: Sharp SM83 / LR35902 (Game Boy) */\n\n")

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
            f.write("/* Function: " + name + " @ " + entry + " */\n")
            f.write(c_code + "\n\n")
            count += 1

f.close()
print("[Ghidra] Decompilation complete! Exported " + str(count) + " functions to " + out_file)
