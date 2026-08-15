#!/usr/bin/env python3
"""Validate the scoped RNG/animation/collision One-on-One manifest."""

import argparse
import json
import subprocess
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RELATIVE_MANIFEST = "docs/parity/ONE_ON_ONE_REMAINING_COVERAGE.json"
MANIFEST = ROOT / RELATIVE_MANIFEST


def manifest_at_ref(ref: str):
    result = subprocess.run(
        ["git", "show", f"{ref}:{RELATIVE_MANIFEST}"],
        cwd=ROOT, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        return None
    return json.loads(result.stdout)


def percentage(data):
    requirements = data["requirements"]
    total = int(data["metric"]["total"])
    verified = sum(item.get("status") == "verified" for item in requirements)
    return verified, total, verified * 100.0 / total


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--json", action="store_true", dest="as_json")
    parser.add_argument("--require-min", type=float)
    parser.add_argument("--baseline-ref")
    parser.add_argument("--require-delta", type=float)
    args = parser.parse_args()

    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    requirements = data["requirements"]
    verified, expected, percent = percentage(data)
    ids = [item.get("id") for item in requirements]
    errors = []

    if len(requirements) != expected:
        errors.append(f"expected {expected} requirements, found {len(requirements)}")
    if len(set(ids)) != len(ids):
        errors.append("requirement IDs are not unique")
    for item in requirements:
        if item.get("status") not in {"verified", "partial", "unmapped"}:
            errors.append(f"{item.get('id')}: invalid status")
        if not item.get("rom"):
            errors.append(f"{item.get('id')}: missing ROM address/data evidence")
        if item.get("status") == "verified" and not item.get("evidence"):
            errors.append(f"{item.get('id')}: verified without evidence")
        for evidence in item.get("evidence", []):
            if not (ROOT / evidence).exists():
                errors.append(f"{item.get('id')}: missing evidence {evidence}")

    groups = {}
    for group in sorted({item["group"] for item in requirements}):
        selected = [item for item in requirements if item["group"] == group]
        counts = Counter(item["status"] for item in selected)
        groups[group] = {
            "verified": counts["verified"],
            "partial": counts["partial"],
            "unmapped": counts["unmapped"],
            "total": len(selected),
            "percent": counts["verified"] * 100.0 / len(selected),
        }

    baseline_percent = None
    delta = None
    if args.baseline_ref:
        baseline = manifest_at_ref(args.baseline_ref)
        if baseline is None:
            history = data.get("history", [])
            if len(history) < 2:
                errors.append("baseline manifest is absent and history has no prior checkpoint")
            else:
                baseline_percent = float(history[-2]["percent"])
        else:
            baseline_percent = percentage(baseline)[2]
        if baseline_percent is not None:
            delta = percent - baseline_percent

    if args.require_delta is not None:
        if delta is None:
            errors.append("--require-delta requires --baseline-ref")
        elif delta + 1e-9 < args.require_delta:
            errors.append(
                f"coverage delta {delta:.2f} is below required {args.require_delta:.2f}")
    if args.require_min is not None and percent + 1e-9 < args.require_min:
        errors.append(f"{percent:.2f}% is below required {args.require_min:.2f}%")

    report = {
        "verified": verified, "total": expected, "percent": round(percent, 2),
        "groups": groups, "baseline_percent": baseline_percent,
        "delta": delta, "errors": errors,
    }
    if args.as_json:
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        print(f"One-on-One remaining coverage: {verified}/{expected} ({percent:.2f}%)")
        for group, values in groups.items():
            print(f"  {group}: {values['verified']}/{values['total']} ({values['percent']:.2f}%)")
        if baseline_percent is not None:
            print(f"  baseline: {baseline_percent:.2f}%  delta: {delta:+.2f} points")
        for error in errors:
            print(f"ERROR: {error}")
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
