#!/usr/bin/env python3
"""
Small benchmark suite for RantanPlan — 14 diverse instances from pddl/small-test/.

Selected from SLURM cluster results to cover:
- Sparse vs dense encoding (mprime vs block-grouping)
- Causal-dominant vs lazy-chain-dominant domains
- Propagator-critical instances (hydropower, satellite)
- Numeric vs boolean domains
- Both families competitive (depots, fo-counters, tpp)

Usage:
    python benchmark_small.py                       # All 6 strategies
    python benchmark_small.py -s pdla               # Single strategy
    python benchmark_small.py --timeout 180         # Custom timeout
    python benchmark_small.py --csv results.csv     # Save CSV
"""
import argparse
import csv
import os
import sys
import time
from pathlib import Path

import unified_planning as up
from unified_planning.io import PDDLReader
from unified_planning.shortcuts import OneshotPlanner
from unified_planning.engines.results import PlanGenerationResultStatus

from rantanplan.planner_wrapper import RantanPlanPlanner

# ---------------------------------------------------------------------------
# Benchmark instances — discovered from pddl/small-test/
# ---------------------------------------------------------------------------

BENCH_DIR = Path("pddl/small-test")

STRATEGIES = [
    "elsc",
    "elsc-fp",
    "pdla",
    "pdla-sa",
]

DEFAULT_TIMEOUT = 120


# ---------------------------------------------------------------------------
# Instance discovery
# ---------------------------------------------------------------------------

def discover_instances():
    """Find all (name, domain_path, problem_path) in pddl/small-test/."""
    instances = []
    for domain_dir in sorted(BENCH_DIR.iterdir()):
        if not domain_dir.is_dir():
            continue
        domain_pddl = domain_dir / "domain.pddl"
        if not domain_pddl.exists():
            continue
        inst_dir = domain_dir / "instances"
        if not inst_dir.exists():
            continue
        for prob in sorted(inst_dir.glob("*.pddl")):
            name = f"{domain_dir.name}/{prob.stem}"
            instances.append((name, str(domain_pddl), str(prob)))
    return instances


# ---------------------------------------------------------------------------
# Runner
# ---------------------------------------------------------------------------

def run_instance(domain_path, problem_path, strategy, timeout, extra_params=None):
    """Run a single (instance, strategy) pair. Returns result dict."""
    stats_file = f"/tmp/bench_{strategy}_{os.getpid()}_{time.monotonic_ns()}.json"

    reader = PDDLReader()
    prob = reader.parse_problem(domain_path, problem_path)

    params = {
        "strategy": strategy,
        "verbosity": "silent",
        "stats_file": stats_file,
    }
    if extra_params:
        params.update(extra_params)

    try:
        with OneshotPlanner(name="RantanPlan", params=params) as p:
            result = p.solve(prob, timeout=timeout)

        solved = result.status in (
            PlanGenerationResultStatus.SOLVED_SATISFICING,
            PlanGenerationResultStatus.SOLVED_OPTIMALLY,
        )
        plan_len = len(result.plan.actions) if solved and result.plan else 0

        stats = {}
        if os.path.exists(stats_file):
            with open(stats_file) as f:
                for line in f:
                    line = line.strip()
                    if ': ' in line:
                        key, val = line.rsplit(': ', 1)
                        try:
                            stats[key] = float(val)
                        except ValueError:
                            pass
            os.unlink(stats_file)

        # Pass timings (preprocessing)
        pass_total = sum(v for k, v in stats.items() if k.startswith("pass.") and k.endswith(".time_ms"))

        return {
            "solved": solved,
            "plan_len": plan_len,
            "time": round(stats.get("planner.total_time", 0), 2),
            "solve_time": round(stats.get("planner.solve_time", 0), 2),
            "preprocess_ms": round(pass_total, 1),
            "horizon": int(stats.get("planner.solution_horizon", 0)),
            "rounds": int(stats.get("planner.rounds", 0)),
            "gr_removed": int(stats.get("goal_relevance.removed_actions", 0)),
            "gr_initial": int(stats.get("goal_relevance.initial_actions", 0)),
            "gr_time_ms": round(stats.get("goal_relevance.time_seconds", 0) * 1000, 1),
            "final_actions": int(stats.get("pipeline.final_actions", 0)),
            "activated": int(stats.get("planner.activated_actions",
                            stats.get("planner.activated_entries", 0))),
            "total_actions": int(stats.get("planner.total_actions",
                                stats.get("pipeline.final_actions", 0))),
            "rlimit": int(stats.get("z3.rlimit count", 0)),
        }

    except Exception as e:
        if os.path.exists(stats_file):
            os.unlink(stats_file)
        return {
            "solved": False,
            "plan_len": 0,
            "time": timeout,
            "horizon": 0,
            "rounds": 0,
            "error": str(e),
        }


# ---------------------------------------------------------------------------
# Display
# ---------------------------------------------------------------------------

G = "\033[92m"  # green
R = "\033[91m"  # red
Y = "\033[93m"  # yellow
B = "\033[1m"   # bold
E = "\033[0m"   # end


def print_header(strategies):
    hdr = f"{'Instance':40s}"
    for s in strategies:
        hdr += f" | {s:>12s}"
    print(f"\n{B}{hdr}{E}")
    print("-" * len(hdr.replace("\033[1m", "").replace("\033[0m", "")))


def print_row(name, results, strategies):
    row = f"{name:40s}"
    for s in strategies:
        r = results.get(s, {})
        if r.get("solved"):
            t = r["time"]
            color = G if t < 30 else (Y if t < 90 else R)
            row += f" | {color}{t:>10.1f}s{E}"
        else:
            row += f" |     {R}{'TO':>5s}{E}  "
    print(row)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def _run_instance_worker(args):
    """Worker for parallel execution."""
    name, domain_path, problem_path, strategy, timeout, extra_params = args
    r = run_instance(domain_path, problem_path, strategy, timeout, extra_params)
    return name, strategy, r


def main():
    parser = argparse.ArgumentParser(description="RantanPlan small benchmark")
    parser.add_argument("-s", "--strategy", action="append",
                        help="Strategy to test (repeatable; default: all 6)")
    parser.add_argument("--timeout", type=int, default=DEFAULT_TIMEOUT,
                        help=f"Timeout per instance in seconds (default: {DEFAULT_TIMEOUT})")
    parser.add_argument("--csv", type=str, default=None,
                        help="Save results to CSV file")
    parser.add_argument("-j", "--jobs", type=int, default=4,
                        help="Number of parallel jobs (default: 4, use 1 for sequential)")
    parser.add_argument("--no-goal-relevance", action="store_true",
                        help="Disable goal-relevance action removal preprocessing (enabled by default)")
    args = parser.parse_args()

    strategies = args.strategy if args.strategy else STRATEGIES
    extra_params = {}
    if args.no_goal_relevance:
        extra_params["no_goal_relevance"] = True

    instances = discover_instances()
    if not instances:
        print(f"No instances found in {BENCH_DIR}/")
        sys.exit(1)

    total = len(instances) * len(strategies)
    print(f"{B}RantanPlan Small Benchmark{E}")
    print(f"{len(instances)} instances x {len(strategies)} strategies = {total} runs")
    print(f"Timeout: {args.timeout}s | Jobs: {args.jobs}\n")

    if extra_params:
        print(f"Extra params: {extra_params}\n")

    # Build work items
    work_items = []
    for name, domain_path, problem_path in instances:
        for s in strategies:
            work_items.append((name, domain_path, problem_path, s, args.timeout, extra_params))

    # Run (parallel or sequential)
    result_map = {}  # (name, strategy) -> result dict
    if args.jobs == 1:
        for i, item in enumerate(work_items, 1):
            name, strategy, r = _run_instance_worker(item)
            result_map[(name, strategy)] = r
            status = f"{G}{r['time']:.1f}s{E}" if r["solved"] else f"{R}TO{E}"
            print(f"  [{i}/{total}] {name} / {strategy}... {status}", flush=True)
    else:
        from concurrent.futures import ProcessPoolExecutor, as_completed
        completed = 0
        with ProcessPoolExecutor(max_workers=args.jobs) as executor:
            futures = {executor.submit(_run_instance_worker, item): item
                       for item in work_items}
            for future in as_completed(futures):
                completed += 1
                name, strategy, r = future.result()
                result_map[(name, strategy)] = r
                status = f"{G}{r['time']:.1f}s{E}" if r["solved"] else f"{R}TO{E}"
                print(f"  [{completed}/{total}] {name} / {strategy}... {status}",
                      flush=True)

    # Display table
    print()
    print_header(strategies)

    all_results = []
    solved_counts = {s: 0 for s in strategies}

    for name, domain_path, problem_path in instances:
        row_results = {}
        for s in strategies:
            r = result_map.get((name, s), {"solved": False, "time": 0})
            row_results[s] = r
            if r.get("solved"):
                solved_counts[s] += 1
            all_results.append({"instance": name, "strategy": s, **r})
        print_row(name, row_results, strategies)

    # Summary
    print()
    print(f"{B}Coverage:{E}")
    for s in strategies:
        n = solved_counts[s]
        pct = 100 * n / len(instances)
        print(f"  {s:>20s}: {n}/{len(instances)} ({pct:.0f}%)")

    # CSV output
    if args.csv:
        with open(args.csv, "w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=[
                "instance", "strategy", "solved", "time", "solve_time",
                "preprocess_ms", "plan_len", "horizon", "rounds",
                "activated", "total_actions", "rlimit",
                "gr_removed", "gr_initial", "gr_time_ms", "final_actions",
            ])
            writer.writeheader()
            for r in all_results:
                writer.writerow({k: r.get(k, "") for k in writer.fieldnames})
        print(f"\nResults saved to {args.csv}")


if __name__ == "__main__":
    main()
