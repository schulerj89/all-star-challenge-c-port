#!/usr/bin/env python3
"""Validate and report focused One-on-One presentation/audio coverage."""

import json
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "docs" / "parity" / "ONE_ON_ONE_PRESENTATION_AUDIO_COVERAGE.json"


def main() -> int:
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
        for evidence in item.get("evidence", []):
            if not (ROOT / evidence).exists():
                errors.append(f"{item['id']}: missing evidence {evidence}")
    verified = sum(item["status"] == "verified" for item in requirements)
    percent = verified * 100.0 / expected
    print(f"One-on-One presentation/audio coverage: {verified}/{expected} ({percent:.2f}%)")
    for group in sorted({item["group"] for item in requirements}):
        selected = [item for item in requirements if item["group"] == group]
        counts = Counter(item["status"] for item in selected)
        group_percent = counts["verified"] * 100.0 / len(selected)
        print(f"  {group}: {counts['verified']}/{len(selected)} ({group_percent:.2f}%)")
    for error in errors:
        print(f"ERROR: {error}")
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
