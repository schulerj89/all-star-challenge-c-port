# Ghidra headless script: recover only reviewed, bank-aware function seeds.

from ghidra.app.cmd.disassemble import DisassembleCommand
from ghidra.app.decompiler import DecompInterface
from ghidra.program.model.symbol import SourceType
from ghidra.util.task import ConsoleTaskMonitor
import json
import os


def function_name(bank, address_text):
    return "rom_b%02d_%s" % (bank, address_text.lower())


args = getScriptArgs()
if len(args) < 2:
    raise RuntimeError("Expected seed JSON and inventory output paths")

seed_path = os.path.abspath(args[0])
inventory_path = os.path.abspath(args[1])
inventory_dir = os.path.dirname(inventory_path)
if not os.path.isdir(inventory_dir):
    os.makedirs(inventory_dir)

with open(seed_path, "r") as source:
    spec = json.load(source)

factory = currentProgram.getAddressFactory()
default_space = factory.getDefaultAddressSpace()
listing = currentProgram.getListing()
functions = currentProgram.getFunctionManager()
monitor = ConsoleTaskMonitor()

decompiler = DecompInterface()
decompiler.openProgram(currentProgram)

default_mapping = spec.get("native_mapping_default", {})
mapping_overrides = spec.get("native_mappings", {})
results = []

for bank_spec in spec.get("banks", []):
    bank = int(bank_spec["bank"])
    requested_space = bank_spec["space"]
    space = default_space if requested_space == "default" else factory.getAddressSpace(requested_space)
    if space is None:
        raise RuntimeError("Address space does not exist: %s" % requested_space)

    for address_text in bank_spec.get("addresses", []):
        normalized_address = address_text.lower()
        address = space.getAddress(int(normalized_address, 16))
        name = function_name(bank, normalized_address)
        errors = []

        command = DisassembleCommand(address, None, True)
        disassembled = bool(command.applyTo(currentProgram, monitor))
        instruction = listing.getInstructionAt(address)
        if instruction is None:
            errors.append("no instruction at seed")

        function = functions.getFunctionAt(address)
        if function is None and instruction is not None:
            try:
                function = createFunction(address, name)
            except Exception as exc:
                errors.append("createFunction failed: %s" % str(exc))
        if function is None:
            containing = functions.getFunctionContaining(address)
            if containing is not None:
                errors.append("seed is inside %s" % containing.getName())
        else:
            try:
                function.setName(name, SourceType.USER_DEFINED)
            except Exception as exc:
                errors.append("rename failed: %s" % str(exc))

        decompile_completed = False
        decompile_message = ""
        body_size = 0
        if function is not None:
            body_size = int(function.getBody().getNumAddresses())
            decompile_result = decompiler.decompileFunction(function, 30, monitor)
            if decompile_result is not None:
                decompile_completed = bool(decompile_result.decompileCompleted())
                decompile_message = decompile_result.getErrorMessage() or ""
            if not decompile_completed:
                errors.append("decompile failed: %s" % decompile_message)

        mapping = dict(default_mapping)
        mapping.update(mapping_overrides.get(name, {}))
        results.append({
            "bank": bank,
            "space": space.getName(),
            "address": normalized_address,
            "name": name,
            "basis": bank_spec.get("basis", ""),
            "disassembled": disassembled or instruction is not None,
            "function_created": function is not None,
            "body_size": body_size,
            "decompile_completed": decompile_completed,
            "decompile_message": decompile_message,
            "native_mapping": mapping,
            "errors": errors,
        })

inventory = {
    "schema_version": 1,
    "program": currentProgram.getName(),
    "processor": currentProgram.getLanguageID().toString(),
    "seed_schema_version": spec.get("schema_version"),
    "rom_sha256": spec.get("rom_sha256"),
    "functions": results,
}

with open(inventory_path, "w") as output:
    json.dump(inventory, output, indent=2, sort_keys=True)
    output.write("\n")

successful = sum(
    item["function_created"] and item["decompile_completed"] and not item["errors"]
    for item in results
)
print(
    "[Ghidra] Recovered and decompiled %d/%d reviewed banked functions; wrote %s"
    % (successful, len(results), inventory_path)
)
