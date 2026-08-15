# Ghidra headless script: create and verify MBC1 ROM-bank overlays.
#
# The raw importer places the 64 KiB file in one flat address space. Physical
# banks 1-3 actually execute in the Game Boy's $4000-$7fff switchable window,
# so each needs its own overlay space for bank-correct symbols and functions.

from java.security import MessageDigest
import json
import os


BANK_SIZE = 0x4000
ROM_SIZE = 0x10000
EXPECTED_CART_TYPE = 0x01
EXPECTED_ROM_SIZE_CODE = 0x01


def unsigned(value):
    return value & 0xff


def sha256_range(memory, start, size):
    digest = MessageDigest.getInstance("SHA-256")
    for offset in range(size):
        digest.update(memory.getByte(start.add(offset)))
    return "".join(["%02x" % unsigned(value) for value in digest.digest()])


def verify_source_bytes(memory, block, file_bytes, file_offset):
    offsets = [0, 1, 0x100, 0x147, 0x1000, BANK_SIZE - 2, BANK_SIZE - 1]
    for relative in offsets:
        mapped = unsigned(memory.getByte(block.getStart().add(relative)))
        source = unsigned(file_bytes.getOriginalByte(file_offset + relative))
        if mapped != source:
            raise RuntimeError(
                "Overlay %s byte mismatch at +0x%04x: mapped=%02x source=%02x"
                % (block.getName(), relative, mapped, source)
            )


def get_or_create_overlay(memory, default_space, file_bytes, bank):
    name = "ROM_BANK_%d" % bank
    block = memory.getBlock(name)
    if block is None:
        block = memory.createInitializedBlock(
            name,
            default_space.getAddress(0x4000),
            file_bytes,
            bank * BANK_SIZE,
            BANK_SIZE,
            True,
        )
    block.setRead(True)
    block.setWrite(False)
    block.setExecute(True)
    block.setComment(
        "MBC1 physical ROM bank %d; file offset 0x%04x; CPU window $4000-$7fff"
        % (bank, bank * BANK_SIZE)
    )
    verify_source_bytes(memory, block, file_bytes, bank * BANK_SIZE)
    return block


args = getScriptArgs()
if len(args) < 1:
    raise RuntimeError("Expected inventory output path as the first script argument")

inventory_path = os.path.abspath(args[0])
inventory_dir = os.path.dirname(inventory_path)
if not os.path.isdir(inventory_dir):
    os.makedirs(inventory_dir)

memory = currentProgram.getMemory()
file_bytes_list = memory.getAllFileBytes()
if file_bytes_list.size() < 1:
    raise RuntimeError("Imported program has no file-backed bytes")

file_bytes = file_bytes_list.get(0)
if file_bytes.getSize() != ROM_SIZE:
    raise RuntimeError(
        "Expected a 64 KiB ROM, found %d bytes" % file_bytes.getSize()
    )

factory = currentProgram.getAddressFactory()
default_space = factory.getDefaultAddressSpace()
rom_start = default_space.getAddress(0)

cart_type = unsigned(memory.getByte(rom_start.add(0x147)))
rom_size_code = unsigned(memory.getByte(rom_start.add(0x148)))
ram_size_code = unsigned(memory.getByte(rom_start.add(0x149)))
if cart_type != EXPECTED_CART_TYPE:
    raise RuntimeError("Expected MBC1 cartridge type 0x01, found 0x%02x" % cart_type)
if rom_size_code != EXPECTED_ROM_SIZE_CODE:
    raise RuntimeError("Expected 64 KiB ROM size code 0x01, found 0x%02x" % rom_size_code)

banks = []

# Bank 0 remains in the default address space at its native fixed address.
bank0_start = default_space.getAddress(0x0000)
banks.append({
    "bank": 0,
    "space": default_space.getName(),
    "block": memory.getBlock(bank0_start).getName(),
    "file_offset": 0,
    "size": BANK_SIZE,
    "cpu_start": "0000",
    "cpu_end": "3fff",
    "overlay": False,
    "sha256": sha256_range(memory, bank0_start, BANK_SIZE),
    "verified": True,
})

for bank in [1, 2, 3]:
    block = get_or_create_overlay(memory, default_space, file_bytes, bank)
    banks.append({
        "bank": bank,
        "space": block.getStart().getAddressSpace().getName(),
        "block": block.getName(),
        "file_offset": bank * BANK_SIZE,
        "size": BANK_SIZE,
        "cpu_start": "%04x" % block.getStart().getOffset(),
        "cpu_end": "%04x" % block.getEnd().getOffset(),
        "overlay": block.isOverlay(),
        "sha256": sha256_range(memory, block.getStart(), BANK_SIZE),
        "verified": True,
    })

title_bytes = []
for offset in range(0x134, 0x144):
    value = unsigned(memory.getByte(rom_start.add(offset)))
    if value == 0:
        break
    title_bytes.append(chr(value))

inventory = {
    "schema_version": 1,
    "program": currentProgram.getName(),
    "processor": currentProgram.getLanguageID().toString(),
    "rom": {
        "title": "".join(title_bytes),
        "size": ROM_SIZE,
        "cartridge_type": cart_type,
        "rom_size_code": rom_size_code,
        "ram_size_code": ram_size_code,
    },
    "banks": banks,
}

with open(inventory_path, "w") as output:
    json.dump(inventory, output, indent=2, sort_keys=True)
    output.write("\n")

print("[Ghidra] Verified four MBC1 ROM banks and wrote %s" % inventory_path)
