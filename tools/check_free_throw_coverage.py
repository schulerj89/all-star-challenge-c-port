#!/usr/bin/env python3
"""Validate and report scoped Free Throw gameplay coverage."""

import argparse
import json
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "docs" / "parity" / "FREE_THROW_COVERAGE.json"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--json", action="store_true", dest="as_json")
    parser.add_argument("--require-min", type=float)
    args = parser.parse_args()
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    requirements = data["requirements"]
    expected = int(data["metric"]["total"])
    errors = []
    ids = [item["id"] for item in requirements]
    if len(requirements) != expected:
        errors.append(f"expected {expected} requirements, found {len(requirements)}")
    if len(set(ids)) != len(ids):
        errors.append("requirement IDs are not unique")
    for item in requirements:
        if item.get("status") not in {"verified", "partial", "unmapped"}:
            errors.append(f"{item.get('id')}: invalid status")
        if item.get("status") == "verified":
            for evidence in item.get("evidence", []):
                if not (ROOT / evidence).exists():
                    errors.append(f"{item['id']}: missing evidence {evidence}")
    verified = sum(item["status"] == "verified" for item in requirements)
    percent = round(verified * 100.0 / expected, 2)
    groups = {}
    for group in sorted({item["group"] for item in requirements}):
        selected = [item for item in requirements if item["group"] == group]
        counts = Counter(item["status"] for item in selected)
        groups[group] = {
            "verified": counts["verified"], "total": len(selected),
            "percent": round(counts["verified"] * 100.0 / len(selected), 2)
        }
    report = {"verified": verified, "total": expected,
              "percent": percent, "groups": groups, "errors": errors}
    if args.as_json:
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        print(f"Free Throw gameplay coverage: {verified}/{expected} ({percent:.2f}%)")
        for group, values in groups.items():
            print(f"  {group}: {values['verified']}/{values['total']} ({values['percent']:.2f}%)")
        for error in errors:
            print(f"ERROR: {error}")
    if errors or (args.require_min is not None and percent < args.require_min):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
