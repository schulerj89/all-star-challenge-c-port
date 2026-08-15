#!/usr/bin/env python3
"""Validate the bank inventory emitted by Ghidra's setup_banked_rom.py."""

import argparse
import json
from pathlib import Path


BANK_SIZE = 0x4000
EXPECTED_OFFSETS = [0x0000, 0x4000, 0x8000, 0xC000]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "inventory",
        nargs="?",
        default="build/ghidra_bank_inventory.json",
        type=Path,
    )
    args = parser.parse_args()

    if not args.inventory.is_file():
        parser.error(f"inventory not found: {args.inventory}")

    data = json.loads(args.inventory.read_text(encoding="utf-8"))
    errors: list[str] = []

    rom = data.get("rom", {})
    if rom.get("size") != 0x10000:
        errors.append(f"ROM size is {rom.get('size')!r}, expected 65536")
    if rom.get("cartridge_type") != 0x01:
        errors.append("cartridge type is not MBC1 (0x01)")
    if rom.get("rom_size_code") != 0x01:
        errors.append("ROM size code is not 0x01 (64 KiB)")
    if rom.get("ram_size_code") != 0x00:
        errors.append("RAM size code is not 0x00 (no cartridge RAM)")

    banks = sorted(data.get("banks", []), key=lambda bank: bank.get("bank", -1))
    if [bank.get("bank") for bank in banks] != [0, 1, 2, 3]:
        errors.append("inventory does not contain exactly banks 0, 1, 2, and 3")
    else:
        hashes: set[str] = set()
        for bank, expected_offset in zip(banks, EXPECTED_OFFSETS):
            bank_number = bank["bank"]
            expected_start = "0000" if bank_number == 0 else "4000"
            expected_end = "3fff" if bank_number == 0 else "7fff"
            if bank.get("file_offset") != expected_offset:
                errors.append(f"bank {bank_number} has wrong file offset")
            if bank.get("size") != BANK_SIZE:
                errors.append(f"bank {bank_number} is not 16 KiB")
            if bank.get("cpu_start") != expected_start or bank.get("cpu_end") != expected_end:
                errors.append(f"bank {bank_number} has wrong CPU address window")
            if bank_number > 0 and bank.get("overlay") is not True:
                errors.append(f"bank {bank_number} is not an overlay")
            if bank.get("verified") is not True:
                errors.append(f"bank {bank_number} was not byte-verified")
            digest = bank.get("sha256", "")
            if len(digest) != 64:
                errors.append(f"bank {bank_number} has no valid SHA-256 digest")
            hashes.add(digest)
        if len(hashes) != 4:
            errors.append("bank digests are not unique")

    if errors:
        print("Ghidra bank verification FAILED:")
        for error in errors:
            print(f"  - {error}")
        return 1

    print("Ghidra bank verification PASSED")
    print(f"  ROM: {rom.get('title', '<unknown>')} ({rom['size']} bytes, MBC1)")
    for bank in banks:
        print(
            f"  Bank {bank['bank']}: file +0x{bank['file_offset']:04x} -> "
            f"{bank['space']}:{bank['cpu_start']}-{bank['cpu_end']}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
