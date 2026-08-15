#!/usr/bin/env python3
"""Validate and report focused One-on-One live-flow coverage."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "docs" / "parity" / "ONE_ON_ONE_LIVE_FLOW_COVERAGE.json"


def main() -> int:
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    requirements = data["requirements"]
    expected = data["metric"]["total"]
    if len(requirements) != expected:
        raise SystemExit(
            f"manifest has {len(requirements)} requirements; expected {expected}")
    missing = []
    for requirement in requirements:
        if requirement.get("status") != "verified":
            missing.append(requirement["id"])
        for evidence in requirement.get("evidence", []):
            if not (ROOT / evidence).exists():
                raise SystemExit(
                    f"missing evidence for {requirement['id']}: {evidence}")
    verified = expected - len(missing)
    percent = verified * 100.0 / expected
    print(f"One-on-One live-flow coverage: {verified}/{expected} ({percent:.2f}%)")
    if missing:
        print("Unverified: " + ", ".join(missing))
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
