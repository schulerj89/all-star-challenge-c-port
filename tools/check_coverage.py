#!/usr/bin/env python3
"""Report strict milestone coverage from docs/COVERAGE_MANIFEST.json.

Only milestones with status ``verified`` receive credit. Identified,
scaffolded, and partial work is visible in the report but receives no partial
credit. This prevents symbol names and incomplete scene shells from inflating
the commit-gate percentage.
"""

import argparse
import json
import subprocess
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = ROOT / "docs" / "COVERAGE_MANIFEST.json"
ALLOWED_STATUSES = {"unmapped", "identified", "scaffolded", "partial", "verified"}


def load_manifest(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def load_manifest_from_git(ref: str) -> dict:
    result = subprocess.run(
        ["git", "show", f"{ref}:docs/COVERAGE_MANIFEST.json"],
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"could not read coverage manifest from {ref!r}: {result.stderr.strip()}"
        )
    return json.loads(result.stdout)


def validate_manifest(data: dict) -> list[str]:
    errors: list[str] = []
    milestones = data.get("milestones", [])
    expected_total = data.get("metric", {}).get("total_milestones")
    if expected_total != len(milestones):
        errors.append(
            f"metric.total_milestones is {expected_total!r}, but {len(milestones)} milestones exist"
        )

    ids = [item.get("id") for item in milestones]
    duplicates = sorted(item_id for item_id, count in Counter(ids).items() if count > 1)
    if duplicates:
        errors.append(f"duplicate milestone ids: {', '.join(duplicates)}")

    for item in milestones:
        item_id = item.get("id", "<missing id>")
        status = item.get("status")
        if status not in ALLOWED_STATUSES:
            errors.append(f"{item_id}: invalid status {status!r}")
        if not item.get("group"):
            errors.append(f"{item_id}: missing group")
        if not item.get("description"):
            errors.append(f"{item_id}: missing description")
        if status == "verified":
            evidence = item.get("evidence", [])
            command = item.get("verification_command")
            if not evidence:
                errors.append(f"{item_id}: verified milestone has no evidence")
            if not command:
                errors.append(f"{item_id}: verified milestone has no verification command")
            for relative in evidence:
                if not (ROOT / relative).exists():
                    errors.append(f"{item_id}: evidence path does not exist: {relative}")

    history = data.get("history", [])
    previous_percent = -1.0
    for entry in history:
        percent = entry.get("percent")
        if not isinstance(percent, (int, float)):
            errors.append("history entry has no numeric percent")
            continue
        if percent < previous_percent:
            errors.append("history percentages must not decrease")
        previous_percent = percent

    return errors


def calculate(data: dict) -> dict:
    milestones = data["milestones"]
    total = len(milestones)
    verified = sum(item["status"] == "verified" for item in milestones)
    percent = round((verified / total) * 100.0, 2) if total else 0.0

    groups: dict[str, dict[str, int | float]] = {}
    for group in sorted({item["group"] for item in milestones}):
        items = [item for item in milestones if item["group"] == group]
        group_verified = sum(item["status"] == "verified" for item in items)
        groups[group] = {
            "verified": group_verified,
            "total": len(items),
            "percent": round((group_verified / len(items)) * 100.0, 2),
        }

    history = data.get("history", [])
    baseline = history[0]["percent"] if history else percent
    previous = history[-2]["percent"] if len(history) > 1 else baseline
    return {
        "verified": verified,
        "total": total,
        "percent": percent,
        "baseline_percent": baseline,
        "previous_percent": previous,
        "delta_from_baseline": round(percent - baseline, 2),
        "delta_from_previous": round(percent - previous, 2),
        "groups": groups,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--json", action="store_true", dest="as_json")
    parser.add_argument("--require-delta", type=float, default=None)
    parser.add_argument(
        "--baseline-ref",
        help="compare against docs/COVERAGE_MANIFEST.json at a Git ref, normally HEAD",
    )
    args = parser.parse_args()

    data = load_manifest(args.manifest)
    errors = validate_manifest(data)
    result = calculate(data)

    if args.baseline_ref:
        try:
            baseline_result = calculate(load_manifest_from_git(args.baseline_ref))
            result["baseline_ref"] = args.baseline_ref
            result["baseline_ref_percent"] = baseline_result["percent"]
            result["delta_from_ref"] = round(
                result["percent"] - baseline_result["percent"], 2
            )
        except (RuntimeError, json.JSONDecodeError) as exc:
            errors.append(str(exc))

    expected_percent = data.get("history", [{}])[-1].get("percent")
    if expected_percent != result["percent"]:
        errors.append(
            f"latest history percent is {expected_percent!r}, calculated {result['percent']:.2f}"
        )

    if args.require_delta is not None:
        gate_delta = result.get("delta_from_ref", result["delta_from_previous"])
        if gate_delta < args.require_delta:
            errors.append(
                f"coverage delta {gate_delta:.2f} is below required "
                f"{args.require_delta:.2f} percentage points"
            )

    if args.as_json:
        print(json.dumps({**result, "errors": errors}, indent=2, sort_keys=True))
    else:
        print("NBA All-Star Challenge verified milestone coverage")
        print(f"  Overall: {result['verified']}/{result['total']} ({result['percent']:.2f}%)")
        for group, values in result["groups"].items():
            print(
                f"  {group}: {values['verified']}/{values['total']} "
                f"({values['percent']:.2f}%)"
            )
        print(f"  Delta from baseline: {result['delta_from_baseline']:+.2f} points")
        print(f"  Delta from previous checkpoint: {result['delta_from_previous']:+.2f} points")
        if "delta_from_ref" in result:
            print(
                f"  Delta from {result['baseline_ref']}: "
                f"{result['delta_from_ref']:+.2f} points"
            )
        if errors:
            print("Coverage validation FAILED:")
            for error in errors:
                print(f"  - {error}")
        else:
            print("Coverage validation PASSED")

    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
