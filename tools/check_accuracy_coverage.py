#!/usr/bin/env python3
"""Validate and report scoped one-player Accuracy Shootout coverage."""
import argparse, json
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "docs" / "parity" / "ACCURACY_COVERAGE.json"

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--json", action="store_true", dest="as_json")
    parser.add_argument("--require-min", type=float)
    args = parser.parse_args()
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    items = data["requirements"]
    expected = int(data["metric"]["total"])
    errors = []
    if len(items) != expected: errors.append(f"expected {expected}, found {len(items)}")
    if len({x["id"] for x in items}) != len(items): errors.append("duplicate IDs")
    for item in items:
        if item["status"] not in {"verified","partial","unmapped"}:
            errors.append(f"{item['id']}: invalid status")
        if item["status"] == "verified":
            for evidence in item.get("evidence", []):
                if not (ROOT / evidence).exists(): errors.append(f"{item['id']}: missing {evidence}")
    verified = sum(x["status"] == "verified" for x in items)
    percent = round(verified * 100.0 / expected, 2)
    groups = {}
    for group in sorted({x["group"] for x in items}):
        selected = [x for x in items if x["group"] == group]
        groups[group] = [sum(x["status"] == "verified" for x in selected), len(selected)]
    report = {"verified":verified,"total":expected,"percent":percent,"groups":groups,"errors":errors}
    if args.as_json: print(json.dumps(report, indent=2, sort_keys=True))
    else:
        print(f"Accuracy Shootout coverage: {verified}/{expected} ({percent:.2f}%)")
        for group, values in groups.items():
            print(f"  {group}: {values[0]}/{values[1]} ({values[0]*100/values[1]:.2f}%)")
        for error in errors: print(f"ERROR: {error}")
    return 1 if errors or (args.require_min is not None and percent < args.require_min) else 0

if __name__ == "__main__": raise SystemExit(main())
