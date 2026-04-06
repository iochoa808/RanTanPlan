#!/usr/bin/env python3
"""
Benchmark comparison: pdla (ExistsPropagator) vs pdla-sa (StateAwareExistsPropagator).

Runs both strategies on all 14 small benchmark instances and compares:
- Cycles detected (propagator.exists_total_cycles)
- Edges skipped (propagator.sa_edges_skipped) — only for pdla-sa
- Solve time, total time
- Z3 rlimit, z3.conflicts, z3.user-propagations
- Plan length, horizon, rounds
"""
import os
import sys
import time
from pathlib import Path

import unified_planning as up
from unified_planning.io import PDDLReader
from unified_planning.engines.results import PlanGenerationResultStatus

from rantanplan.planner_wrapper import RantanPlanPlanner

BENCH_DIR = Path("pddl/small-test")
STRATEGIES = ["pdla", "pdla-sa"]
DEFAULT_TIMEOUT = 120

STAT_KEYS = [
    "planner.total_time",
    "planner.solve_time",
    "planner.solution_horizon",
    "planner.rounds",
    "planner.activated_actions",
    "propagator.exists_total_cycles",
    "propagator.sa_edges_skipped",
    "propagator.sa_registered_pairs",
    "propagator.sa_edge_lits",
    "z3.rlimit count",
    "z3.conflicts",
    "z3.user-propagations",
]


def discover_instances():
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


def run_instance(domain_path, problem_path, strategy, timeout):
    stats_file = f"/tmp/bench_sa_{strategy}_{os.getpid()}_{time.monotonic_ns()}.json"
    reader = PDDLReader()
    prob = reader.parse_problem(domain_path, problem_path)

    params = {
        "strategy": strategy,
        "verbosity": "silent",
        "stats_file": stats_file,
    }

    try:
        with up.shortcuts.OneshotPlanner(name="RantanPlan", params=params) as p:
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

        return {
            "solved": solved,
            "plan_len": plan_len,
            **{k: stats.get(k, 0) for k in STAT_KEYS},
        }

    except Exception as e:
        if os.path.exists(stats_file):
            os.unlink(stats_file)
        return {
            "solved": False,
            "plan_len": 0,
            "error": str(e),
            **{k: 0 for k in STAT_KEYS},
        }


G = "\033[92m"
R = "\033[91m"
Y = "\033[93m"
B = "\033[1m"
E = "\033[0m"


def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--timeout", type=int, default=DEFAULT_TIMEOUT)
    args = parser.parse_args()

    instances = discover_instances()
    print(f"Found {len(instances)} instances\n")

    all_results = {}

    for name, dom, prob in instances:
        print(f"{B}{name}{E}")
        all_results[name] = {}
        for strategy in STRATEGIES:
            sys.stdout.write(f"  {strategy:12s} ... ")
            sys.stdout.flush()
            r = run_instance(dom, prob, strategy, args.timeout)
            all_results[name][strategy] = r
            if r["solved"]:
                print(f"{G}solved{E} "
                      f"t={r['planner.total_time']:.1f}s "
                      f"cyc={int(r['propagator.exists_total_cycles'])} "
                      f"skip={int(r.get('propagator.sa_edges_skipped', 0))} "
                      f"rlim={int(r['z3.rlimit count'])}")
            else:
                print(f"{R}TIMEOUT{E}")

    # Summary table
    print(f"\n\n{'=' * 120}")
    print(f"{B}{'COMPARISON SUMMARY':^120s}{E}")
    print(f"{'=' * 120}")

    hdr = (f"{'Instance':32s} | {'Strategy':10s} | {'Solved':6s} | {'Time':>7s} | "
           f"{'Cycles':>7s} | {'Skipped':>8s} | {'EdgeLits':>8s} | "
           f"{'Conflicts':>9s} | {'UserProp':>8s} | {'Rlimit':>12s} | "
           f"{'Plan':>4s} | {'Hor':>3s}")
    print(hdr)
    print("-" * 120)

    for name in [n for n, _, _ in instances]:
        for strategy in STRATEGIES:
            r = all_results[name].get(strategy, {})
            solved_str = f"{G}Yes{E}" if r.get("solved") else f"{R}No{E} "
            t = r.get("planner.total_time", 0)
            cyc = int(r.get("propagator.exists_total_cycles", 0))
            skip = int(r.get("propagator.sa_edges_skipped", 0))
            elits = int(r.get("propagator.sa_edge_lits", 0))
            conflicts = int(r.get("z3.conflicts", 0))
            uprop = int(r.get("z3.user-propagations", 0))
            rlim = int(r.get("z3.rlimit count", 0))
            plen = r.get("plan_len", 0)
            hor = int(r.get("planner.solution_horizon", 0))

            label = "sa" if strategy == "pdla-sa" else "pdla"
            row = (f"{name:32s} | {label:10s} | {solved_str} | "
                   f"{t:>6.1f}s | {cyc:>7d} | {skip:>8d} | {elits:>8d} | "
                   f"{conflicts:>9d} | {uprop:>8d} | {rlim:>12d} | "
                   f"{plen:>4d} | {hor:>3d}")
            print(row)
        print("-" * 120)


if __name__ == "__main__":
    main()
