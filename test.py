#!/usr/bin/env python3
"""
Comprehensive test script for the RantanPlan planning system.

Discovers all PDDL problems in pddl/test/ and runs the planner with every
available strategy, validating each resulting plan. Strategies are auto-
discovered from the C++ binary via --list-strategies.

Output style: one compact line per test, with a summary at the end.
"""
import os
import sys
import argparse
import subprocess
from pathlib import Path

import unified_planning as up
from unified_planning.io import PDDLReader
from unified_planning.shortcuts import OneshotPlanner, PlanValidator
from unified_planning.engines.results import PlanGenerationResultStatus
from unified_planning.engines import ValidationResultStatus

from rantanplan.planner_wrapper import RantanPlanPlanner

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

QUICK_TEST_DIRS = [
    "pddl/test/zenotravel",
    "pddl/test/rover",
    "pddl/test/gripper-round-1-adl",
    "pddl/test/hydropower",
    "pddl/test/sdac-simple",
    "pddl/test/sdac-zero-bound",
    "pddl/test/worksworld",
]

# Strategies tested in --quick mode. Covers each major code path without
# redundant variants (eager-syntactic, eager-semantic, non-chained lazy, etc.).
QUICK_STRATEGIES = [
    "seq",                              # Sequential baseline, Grounded encoder
    "r2e",                              # R2E encoder, declaration-order parallelism
    "forall-lazy-semantic-chain",       # Forall, Chained encoder, lazy semantic, LazyForall propagator
    "forall-lazy-semantic-chain-dt",    # DoubleTail planner
    "exists-lazy-semantic-chain",       # Exists, Chained encoder, lazy semantic, Exists propagator
    "exists-lazy-semantic-chain-dt",    # DoubleTail exists
    "exists-lazy-semantic-chain-fp",    # Frame propagator variant
    "exists-lazy-semantic-chain-foot",  # Footprint-indexed exists
    "causal-exists",                    # CausalExists planner kind
    "causal-exists-fp",                 # CausalExists + frame propagator
    "causal-exists-guided",            # CausalExists + guided achiever selection + tracked preconditions
    "pdla",                                 # PDLA: property-directed lazy activation
]

TIMEOUT = 60  # seconds per test

# ---------------------------------------------------------------------------
# ANSI helpers
# ---------------------------------------------------------------------------

class C:
    GREEN  = '\033[92m'
    RED    = '\033[91m'
    YELLOW = '\033[93m'
    BOLD   = '\033[1m'
    END    = '\033[0m'


# ---------------------------------------------------------------------------
# Discovery
# ---------------------------------------------------------------------------

def get_strategies():
    """Query the C++ binary for available strategies."""
    planner = RantanPlanPlanner()
    result = subprocess.run(
        [planner.executable_path, "/dev/null", "/dev/null", "--list-strategies"],
        capture_output=True, text=True, check=True,
    )
    return [
        line.strip()
        for line in result.stdout.strip().split('\n')
        if line.strip() and not line.strip().startswith('Available')
    ]


def find_problems(quick=False):
    """Return sorted list of (name, domain_path, problem_path)."""
    problems = []
    search_dirs = QUICK_TEST_DIRS if quick else ["pddl/test"]
    for search_dir in search_dirs:
        for dirpath, _, filenames in os.walk(search_dir):
            domain = problem = None
            for f in filenames:
                if "domain" in f.lower() and f.endswith(".pddl"):
                    domain = Path(dirpath) / f
                elif "problem" in f.lower() and f.endswith(".pddl"):
                    problem = Path(dirpath) / f
            if domain and problem:
                problems.append((Path(dirpath).name, domain, problem))
    return sorted(problems, key=lambda t: t[0])


# ---------------------------------------------------------------------------
# Single test
# ---------------------------------------------------------------------------

def run_test(name, domain, problem, strategy, verbose=False, up_grounding=False, mode=None):
    """Run one test. Returns (passed: bool, detail: str)."""
    try:
        reader = PDDLReader()
        prob = reader.parse_problem(str(domain), str(problem))

        params = {
            'strategy': strategy,
            'verbosity': 'info' if verbose else 'silent',
        }
        if up_grounding:
            params['up_grounding'] = True
        if mode is not None:
            params['mode'] = mode

        engine_name = 'RantanPlan-optimal' if mode == 'optimal' else 'RantanPlan'
        with OneshotPlanner(name=engine_name, params=params) as planner:
            result = planner.solve(prob, timeout=TIMEOUT)

        if result.status not in (PlanGenerationResultStatus.SOLVED_SATISFICING,
                                  PlanGenerationResultStatus.SOLVED_OPTIMALLY):
            return False, result.status.name

        if result.plan is None:
            return False, "NO_PLAN"

        val = PlanValidator()
        vr = val.validate(prob, result.plan)
        if vr.status == ValidationResultStatus.VALID:
            return True, f"{len(result.plan.actions)} actions"
        else:
            return False, "INVALID_PLAN"

    except Exception as e:
        return False, str(e)[:80]


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

## Strategies to test with --mode optimal (replaces the old -opt registry entries)
OPTIMAL_MODE_STRATEGIES = [
    "seq", "r2e",
    "forall", "forall-prop", "forall-lazy", "forall-lazy-semantic", "forall-lazy-semantic-chain",
    "exists", "exists-lazy", "exists-lazy-semantic", "exists-lazy-semantic-chain",
]

QUICK_OPTIMAL_MODE_STRATEGIES = [
    "seq",
    "exists-lazy-semantic-chain",
]


def build_test_configs(strategies, quick=False):
    """Build list of (strategy, mode, label) tuples to test."""
    configs = []
    for s in strategies:
        configs.append((s, None, s))
    optimal_pool = QUICK_OPTIMAL_MODE_STRATEGIES if quick else OPTIMAL_MODE_STRATEGIES
    for s in optimal_pool:
        if s in strategies:
            configs.append((s, "optimal", f"{s} --mode optimal"))
    return configs


def main():
    parser = argparse.ArgumentParser(description="RantanPlan test suite")
    parser.add_argument("--quick", action="store_true",
                        help="Test only a small subset of domains")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Show planner output")
    parser.add_argument("--up-grounding", action="store_true",
                        help="Use UP cross-product grounding instead of C++ grounding")
    parser.add_argument("-j", "--jobs", type=int, default=4,
                        help="Number of parallel jobs (default: 4, use 1 for sequential)")
    args = parser.parse_args()

    all_strategies = get_strategies()
    if args.quick:
        strategies = [s for s in QUICK_STRATEGIES if s in all_strategies]
    else:
        strategies = all_strategies
    problems = find_problems(quick=args.quick)

    if not problems:
        print("No PDDL problems found.")
        sys.exit(1)

    configs = build_test_configs(strategies, quick=args.quick)
    total = len(problems) * len(configs)
    print(f"\n{C.BOLD}RantanPlan Test Suite{C.END}")
    print(f"{len(problems)} domains x {len(configs)} configs = {total} tests")
    print(f"Jobs: {args.jobs}\n")

    # Build work items: (index, test_label, args_for_run_test)
    work_items = []
    for name, domain, problem_file in problems:
        for strategy, mode, label in configs:
            test_label = f"{name} / {label}"
            work_items.append((test_label, name, domain, problem_file, strategy,
                               args.verbose, args.up_grounding, mode))

    if args.jobs == 1 or args.verbose:
        # Sequential: print as we go (verbose needs sequential for readable output)
        results = _run_sequential(work_items, total)
    else:
        results = _run_parallel(work_items, total, args.jobs)

    # Summary
    passed = sum(1 for ok, _ in results.values() if ok)
    failed = sum(1 for ok, _ in results.values() if not ok)
    failures = [(label, detail) for label, (ok, detail) in results.items() if not ok]

    print(f"\n{'=' * 60}")
    print(f"{C.BOLD}Test Summary{C.END}")
    print(f"{'=' * 60}")
    print(f"  Total:  {passed + failed}")
    print(f"  Passed: {C.GREEN}{passed}{C.END}")
    print(f"  Failed: {C.RED}{failed}{C.END}")

    if failures:
        print(f"\n{C.RED}Failed tests:{C.END}")
        for label, detail in failures:
            print(f"  {label}: {detail}")

    sys.exit(1 if failed else 0)


def _run_sequential(work_items, total):
    """Run tests sequentially with live output."""
    results = {}
    for i, (test_label, name, domain, problem_file, strategy,
            verbose, up_grounding, mode) in enumerate(work_items, 1):
        print(f"  [{i}/{total}] {test_label}... ", end="", flush=True)
        ok, detail = run_test(name, domain, problem_file, strategy,
                              verbose=verbose, up_grounding=up_grounding, mode=mode)
        if ok:
            print(f"{C.GREEN}PASS{C.END} ({detail})")
        else:
            print(f"{C.RED}FAIL{C.END} ({detail})")
        results[test_label] = (ok, detail)
    return results


def _run_test_worker(args):
    """Worker for parallel execution. Unpacks args and calls run_test."""
    test_label, name, domain, problem_file, strategy, verbose, up_grounding, mode = args
    ok, detail = run_test(name, domain, problem_file, strategy,
                          verbose=verbose, up_grounding=up_grounding, mode=mode)
    return test_label, ok, detail


def _run_parallel(work_items, total, jobs):
    """Run tests in parallel using ProcessPoolExecutor."""
    from concurrent.futures import ProcessPoolExecutor, as_completed

    results = {}
    completed = 0

    with ProcessPoolExecutor(max_workers=jobs) as executor:
        futures = {executor.submit(_run_test_worker, item): item[0]
                   for item in work_items}

        for future in as_completed(futures):
            completed += 1
            test_label, ok, detail = future.result()
            results[test_label] = (ok, detail)
            status = f"{C.GREEN}PASS{C.END}" if ok else f"{C.RED}FAIL{C.END}"
            print(f"  [{completed}/{total}] {test_label}... {status} ({detail})")

    return results


if __name__ == "__main__":
    sys.path.insert(0, os.path.abspath(os.path.dirname(__file__)))
    main()
