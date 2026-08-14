#!/usr/bin/env python3
"""
NBA All-Star Challenge - Ghidra & Assembly Coverage Verification Tool
Scans the C port codebase against the Ghidra/Assembly mapping matrix.
"""

import os
import re
import sys

DOC_PATH = os.path.join(os.path.dirname(__file__), "..", "docs", "GHIDRA_COVERAGE.md")
SRC_DIR = os.path.join(os.path.dirname(__file__), "..", "src")
INC_DIR = os.path.join(os.path.dirname(__file__), "..", "include")

def parse_coverage_doc():
    if not os.path.exists(DOC_PATH):
        print(f"Error: Coverage doc not found at {DOC_PATH}")
        sys.exit(1)
        
    with open(DOC_PATH, "r", encoding="utf-8") as f:
        content = f.read()

    # Match markdown table rows: | `Routine` | Bank/Addr | Desc | `Symbol` | `File` |
    pattern = r'\|\s*`([^`]+)`\s*\|\s*([^|]+)\|\s*([^|]+)\|\s*`([^`]+)`\s*\|\s*`?([^`|\n]+)`?\s*\|'
    matches = re.findall(pattern, content)
    
    entries = []
    for m in matches:
        routine = m[0].strip()
        bank_addr = m[1].strip()
        desc = m[2].strip()
        symbol = m[3].strip()
        source_file = m[4].strip()
        if routine != "ROM Routine": # Header skip
            entries.append({
                "routine": routine,
                "addr": bank_addr,
                "desc": desc,
                "symbol": symbol,
                "file": source_file
            })
    return entries

def scan_codebase_symbols():
    symbols = set()
    for root_dir in [SRC_DIR, INC_DIR]:
        for root, _, files in os.walk(root_dir):
            for file in files:
                if file.endswith((".c", ".h")):
                    path = os.path.join(root, file)
                    with open(path, "r", encoding="utf-8", errors="ignore") as f:
                        text = f.read()
                        # Extract identifiers and comments
                        for word in re.findall(r'[A-Za-z0-9_]+', text):
                            symbols.add(word)
    return symbols

def main():
    print("=" * 70)
    print("  NBA ALL-STAR CHALLENGE (GAME BOY) -> C PORT GHIDRA COVERAGE")
    print("=" * 70)

    entries = parse_coverage_doc()
    code_symbols = scan_codebase_symbols()

    total = len(entries)
    implemented = 0
    missing = []

    print(f"\n[+] Scanning {total} mapped Game Boy ROM routines across C codebase...")

    for e in entries:
        found = (e["symbol"] in code_symbols) or (e["routine"] in code_symbols)
        status = "IMPLEMENTED" if found else "MISSING"
        if found:
            implemented += 1
            print(f"  [OK] {e['routine']:<18} ({e['addr']:<16}) -> {e['symbol']:<30}")
        else:
            missing.append(e)
            print(f"  [--] {e['routine']:<18} ({e['addr']:<16}) -> {e['symbol']:<30} [MISSING]")

    coverage_pct = (implemented / total * 100.0) if total > 0 else 0.0

    print("\n" + "=" * 70)
    print(f"  SUMMARY: {implemented}/{total} ROUTINES COVERED ({coverage_pct:.1f}%)")
    print("=" * 70)

    if missing:
        print("\nMissing items:")
        for m in missing:
            print(f"  - {m['routine']} ({m['symbol']} in {m['file']})")
        sys.exit(1)
    else:
        print("\nAll Ghidra/Assembly mapped routines successfully verified in Native C Port!")
        sys.exit(0)

if __name__ == "__main__":
    main()
