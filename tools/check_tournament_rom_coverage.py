#!/usr/bin/env python3
"""Report native coverage of the tournament-exclusive ROM routines.

Reads docs/parity/TOURNAMENT_ROM_COVERAGE.json, which is a ledger of the 64
routines reachable only from menu mode $04 ($0F2E).  A routine counts as ported
only when the ledger names a C source file that exists and actually mentions the
routine's address, so a stale entry fails instead of silently inflating the
number.  Exit code is non-zero if any 'ported' entry cannot be substantiated.
"""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LEDGER = os.path.join(ROOT, "docs", "parity", "TOURNAMENT_ROM_COVERAGE.json")


def main() -> int:
    with open(LEDGER, encoding="utf-8") as handle:
        ledger = json.load(handle)

    total_bytes = ledger["metric"]["total_bytes"]
    total_routines = ledger["metric"]["total_routines"]
    ported_bytes = 0
    ported_routines = 0
    problems = []
    by_chunk = {}

    for entry in ledger["routines"]:
        if entry["status"] != "ported":
            continue
        source = entry.get("source")
        if not source:
            problems.append("%s is marked ported but names no source" % entry["id"])
            continue
        path = os.path.join(ROOT, source.replace("/", os.sep))
        if not os.path.isfile(path):
            problems.append("%s names a missing source %s" % (entry["id"], source))
            continue
        with open(path, encoding="utf-8", errors="replace") as handle:
            text = handle.read()
        if entry["address"] not in text:
            problems.append("%s is not referenced in %s" % (entry["address"], source))
            continue
        ported_bytes += entry["bytes"]
        ported_routines += 1
        chunk = entry.get("chunk", 0)
        by_chunk.setdefault(chunk, [0, 0])
        by_chunk[chunk][0] += 1
        by_chunk[chunk][1] += entry["bytes"]

    print("Tournament-exclusive ROM code (menu mode $04, driver $0F2E)")
    print("  routines ported : %d / %d" % (ported_routines, total_routines))
    print("  bytes ported    : %d / %d  (%.1f%%)"
          % (ported_bytes, total_bytes, 100.0 * ported_bytes / total_bytes))

    for chunk in sorted(by_chunk):
        title = next((c["title"] for c in ledger.get("chunks", []) if c["id"] == chunk), "chunk %d" % chunk)
        count, size = by_chunk[chunk]
        print("    chunk %d  %-24s %2d routines %5d bytes  (%.1f%% of the mode)"
              % (chunk, title, count, size, 100.0 * size / total_bytes))

    pending = [e for e in ledger["routines"] if e["status"] != "ported"]
    pending.sort(key=lambda e: -e["bytes"])
    print("  largest pending :")
    for entry in pending[:8]:
        print("    %-8s %4d bytes %4d instr" % (entry["address"], entry["bytes"], entry["instructions"]))

    if problems:
        print("")
        for problem in problems:
            print("LEDGER ERROR: %s" % problem)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
