#!/usr/bin/env python3
"""tests/behavioral/run_tests.py

Dependency-free Gherkin runner for the behavioural (app + UI) layer.

Parses *.feature files, executes Background + Scenario steps by matching
them to step functions in tests/behavioral/steps/test_steps.py, and
reports pass/fail per scenario. No pytest/pytest-bdd/behave required.

Usage:
    python3 tests/behavioral/run_tests.py [features/artha.feature ...]

Exit code: 0 if all scenarios pass, 1 otherwise.
"""

from __future__ import annotations

import sys
from pathlib import Path

# Make `steps` importable when run from the repo root.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from steps.test_steps import find_step  # noqa: E402


FEATURE_DIR = Path(__file__).resolve().parent / "features"


def parse_feature(path: Path):
    """Yield (scenario_name, [steps]) where each step is (keyword, text)."""
    scenario = None
    steps = []
    background_steps = []
    in_background = False
    with path.open() as fh:
        for raw in fh:
            line = raw.strip()
            if not line or line.startswith("#") or line.startswith("Feature:"):
                continue
            if line.startswith("Background:"):
                in_background = True
                continue
            if line.startswith("Scenario:"):
                # flush previous scenario
                if scenario is not None:
                    yield scenario, background_steps + steps
                scenario = line.split(":", 1)[1].strip()
                steps = []
                in_background = False
                continue
            for kw in ("Given", "When", "Then", "And", "But"):
                if line.startswith(kw + " "):
                    step = (kw, line[len(kw) :].strip())
                    if in_background:
                        background_steps.append(step)
                    else:
                        steps.append(step)
                    break
    # flush last scenario
    if scenario is not None:
        yield scenario, background_steps + steps


def run_scenario(scenario: str, steps: list) -> list:
    """Run one scenario's steps; return list of (step, ok, error) tuples."""
    results = []
    for keyword, text in steps:
        fn = find_step(keyword, text)
        if fn is None:
            results.append((f"{keyword}: {text}", False, "no step definition found"))
            continue
        try:
            fn()
            results.append((f"{keyword}: {text}", True, None))
        except Exception as exc:  # noqa: BLE001
            results.append((f"{keyword}: {text}", False, str(exc)))
    return results


def main() -> int:
    feature_files = [Path(p) for p in sys.argv[1:]]
    if not feature_files:
        feature_files = sorted(FEATURE_DIR.glob("*.feature"))
    if not feature_files:
        print("no feature files found", file=sys.stderr)
        return 1

    overall_ok = True
    for feat_path in feature_files:
        print(f"== Feature: {feat_path.name}")
        for scenario, steps in parse_feature(feat_path):
            print(f"  Scenario: {scenario}")
            scenario_ok = True
            for step_desc, ok, err in run_scenario(scenario, steps):
                mark = "ok" if ok else "FAIL"
                print(f"    [{mark}] {step_desc}")
                if err:
                    print(f"        {err}")
                if not ok:
                    scenario_ok = False
                    overall_ok = False
            if scenario_ok:
                print(f"    => Scenario PASSED: {scenario}")
            else:
                print(f"    => Scenario FAILED: {scenario}")
    return 0 if overall_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
