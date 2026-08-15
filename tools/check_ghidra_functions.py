#!/usr/bin/env python3
"""Validate the reviewed banked-function inventory emitted by Ghidra."""

import argparse
from collections import Counter
import json
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SEEDS = ROOT / "tools" / "ghidra" / "function_seeds.json"
DEFAULT_INVENTORY = ROOT / "build" / "ghidra_function_inventory.json"
ALLOWED_MAPPING_STATUSES = {"unmapped", "candidate", "scaffolded", "partial", "verified"}


def expected_functions(spec: dict) -> dict[str, tuple[int, str]]:
    expected: dict[str, tuple[int, str]] = {}
    for bank_spec in spec.get("banks", []):
        bank = int(bank_spec["bank"])
        for address in bank_spec.get("addresses", []):
            normalized = address.lower()
            name = f"rom_b{bank:02d}_{normalized}"
            expected[name] = (bank, normalized)
    return expected


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("inventory", nargs="?", type=Path, default=DEFAULT_INVENTORY)
    parser.add_argument("--seeds", type=Path, default=DEFAULT_SEEDS)
    args = parser.parse_args()

    if not args.seeds.is_file():
        parser.error(f"seed spec not found: {args.seeds}")
    if not args.inventory.is_file():
        parser.error(f"inventory not found: {args.inventory}")

    spec = json.loads(args.seeds.read_text(encoding="utf-8"))
    inventory = json.loads(args.inventory.read_text(encoding="utf-8"))
    expected = expected_functions(spec)
    expected_counts = Counter(bank for bank, _address in expected.values())
    actual_items = inventory.get("functions", [])
    actual = {item.get("name"): item for item in actual_items}
    errors: list[str] = []

    if inventory.get("rom_sha256") != spec.get("rom_sha256"):
        errors.append("inventory ROM digest does not match the checked-in seed spec")
    if set(actual) != set(expected):
        missing = sorted(set(expected) - set(actual))
        extra = sorted(set(actual) - set(expected))
        if missing:
            errors.append(f"missing seed results: {', '.join(missing)}")
        if extra:
            errors.append(f"unexpected seed results: {', '.join(extra)}")

    counts = Counter(item.get("bank") for item in actual_items)
    if counts != expected_counts:
        errors.append(f"bank counts are {dict(counts)}, expected {dict(expected_counts)}")

    for name, (expected_bank, expected_address) in expected.items():
        item = actual.get(name)
        if item is None:
            continue
        expected_space = "ram" if expected_bank == 0 else f"ROM_BANK_{expected_bank}"
        if item.get("bank") != expected_bank or item.get("address") != expected_address:
            errors.append(f"{name}: bank/address mismatch")
        if item.get("space") != expected_space:
            errors.append(f"{name}: space is {item.get('space')!r}, expected {expected_space!r}")
        if item.get("disassembled") is not True:
            errors.append(f"{name}: seed was not disassembled")
        if item.get("function_created") is not True:
            errors.append(f"{name}: function was not created")
        if item.get("decompile_completed") is not True:
            errors.append(f"{name}: decompilation did not complete")
        if item.get("body_size", 0) <= 0:
            errors.append(f"{name}: function body is empty")
        if item.get("errors"):
            errors.append(f"{name}: {'; '.join(item['errors'])}")

        mapping = item.get("native_mapping", {})
        status = mapping.get("status")
        if status not in ALLOWED_MAPPING_STATUSES:
            errors.append(f"{name}: invalid native mapping status {status!r}")
        if status != "unmapped":
            source = mapping.get("source")
            symbol = mapping.get("native_symbol")
            if not source or not (ROOT / source).is_file():
                errors.append(f"{name}: mapped source does not exist: {source!r}")
            if not symbol:
                errors.append(f"{name}: mapped entry has no native symbol")
            elif source and (ROOT / source).is_file():
                source_text = (ROOT / source).read_text(encoding="utf-8")
                if re.search(rf"\b{re.escape(symbol)}\s*\(", source_text) is None:
                    errors.append(f"{name}: native symbol {symbol!r} is absent from {source}")

    if errors:
        print("Ghidra function recovery FAILED:")
        for error in errors:
            print(f"  - {error}")
        return 1

    mapping_counts = Counter(
        item["native_mapping"]["status"] for item in actual_items
    )
    print("Ghidra function recovery PASSED")
    print(f"  Reviewed functions: {len(actual_items)}")
    for bank in sorted(expected_counts):
        print(f"  Bank {bank}: {counts[bank]} recovered and decompiled")
    print(
        "  Native mapping status: "
        + ", ".join(f"{key}={value}" for key, value in sorted(mapping_counts.items()))
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
