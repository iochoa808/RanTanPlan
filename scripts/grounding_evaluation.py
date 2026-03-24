#!/usr/bin/env python3
"""
grounding_evaluation.py — Evaluate grounding improvements across bench instances.

For each PDDL instance in pddl/bench/ it measures the action count produced by
four pipeline configurations:

  1. UP cross-product        --up-grounding --no-action-removal  (naive, no pruning)
  2. UP + BoolRPG            --up-grounding                      (UP grounder + boolean RPG)
  3. C++ grounder            (default, no extra flags)           (delete-relaxation fixpoint, boolean only)
  4. C++ + numeric grounding --numeric-grounding                 (grounder + interval-based numeric pruning)

The planner is invoked via RantanPlanPlanner (like benchmark.py) with a short
--timeout so the binary exits after the pipeline rather than spending time on search.
`pipeline.final_actions` (written to a temp stats file) captures the final action
count after all passes in all four cases.

Usage
-----
    python grounding_evaluation.py
    python grounding_evaluation.py --bench-dir pddl/bench --timeout 5
"""

import argparse
import os
import sys
import tempfile
from pathlib import Path

from rantanplan.planner_wrapper import RantanPlanPlanner
from unified_planning.io import PDDLReader


# ---------------------------------------------------------------------------
# Configurations to compare
# ---------------------------------------------------------------------------

CONFIGS = [
    ("UP cross-product",    dict(up_grounding=True, no_action_removal=True)),
    ("UP + BoolRPG",        dict(up_grounding=True)),
    ("C++ grounder",        dict()),
    ("C++ + numeric grnd",  dict(numeric_grounding=True)),
]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def find_bench_instances(bench_dir: str):
    """Yield (domain_name, domain_file, problem_file) for every bench instance."""
    for domain_dir in sorted(Path(bench_dir).iterdir()):
        if not domain_dir.is_dir() or domain_dir.name.startswith('.'):
            continue
        domain_file = domain_dir / "domain.pddl"
        instances_dir = domain_dir / "instances"
        if not domain_file.exists() or not instances_dir.exists():
            continue
        for problem_file in sorted(instances_dir.glob("*.pddl")):
            yield domain_dir.name, str(domain_file), str(problem_file)


def _parse_stats(path: str) -> dict:
    result = {}
    try:
        with open(path) as fh:
            for line in fh:
                if ':' in line:
                    k, _, v = line.partition(':')
                    try:
                        result[k.strip()] = float(v.strip())
                    except ValueError:
                        pass
    except OSError:
        pass
    return result


def run_config(domain_file: str, problem_file: str, timeout: int,
               extra_opts: dict) -> "int | None | str":
    """
    Run the planner with a given set of extra options.
    Returns pipeline.final_actions (int), None if no stat was written, or an
    error string if an exception was raised before the binary could run.
    """
    with tempfile.NamedTemporaryFile(suffix=".txt", delete=False) as sf:
        stats_path = sf.name

    error_msg = None
    try:
        planner = RantanPlanPlanner(
            strategy="seq",
            verbosity="silent",
            stats_file=stats_path,
            max_steps=1,      # exit after at most 1 solver step — pipeline stats are always written
            **extra_opts,
        )
        reader = PDDLReader()
        problem = reader.parse_problem(domain_file, problem_file)
        planner._solve(problem, timeout=float(timeout))
    except Exception as e:
        error_msg = type(e).__name__ + ": " + str(e).split("\n")[0]

    stats = _parse_stats(stats_path)
    try:
        os.unlink(stats_path)
    except OSError:
        pass

    val = stats.get("pipeline.final_actions")
    if val is not None:
        return int(val)
    return error_msg if error_msg else None


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Evaluate grounding improvements across bench instances.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--bench-dir", default="pddl/bench",
                        help="Root benchmark directory")
    parser.add_argument("--timeout", type=int, default=5,
                        help="Per-run timeout (seconds) passed to the C++ binary")
    args = parser.parse_args()

    instances = list(find_bench_instances(args.bench_dir))
    if not instances:
        print(f"No instances found under {args.bench_dir!r}", file=sys.stderr)
        sys.exit(1)

    n_inst = len(instances)
    col_names = [name for name, _ in CONFIGS]
    n_configs = len(CONFIGS)

    print(f"Instances : {n_inst}   Timeout per run: {args.timeout}s")
    print(f"Configs   : {', '.join(col_names)}\n")

    rows = []  # (domain, instance, counts[4])

    for idx, (domain, domain_file, problem_file) in enumerate(instances, 1):
        instance = Path(problem_file).stem
        counts = []
        for cfg_name, opts in CONFIGS:
            n = run_config(domain_file, problem_file, args.timeout, opts)
            counts.append(n if isinstance(n, int) else None)
            status = str(n) if isinstance(n, int) else f"ERR: {n}"
            print(f"  [{idx:2d}/{n_inst}] {domain}/{instance:<20} {cfg_name:<22} → {status}")
        rows.append((domain, instance, counts))

    # --- Summary table ---
    W0, W1 = 16, 20
    CW = 15

    def fmt_cell(v) -> str:
        return f"{str(v) if v is not None else 'N/A':>{CW}}"

    def fmt_row(d, i, cells) -> str:
        return f"{d:<{W0}} {i:<{W1}}" + "".join(f" {c}" for c in cells)

    sep_w = W0 + 1 + W1 + n_configs * (CW + 1)
    sep = "-" * sep_w

    print()
    print("=" * sep_w)
    print(fmt_row("Domain", "Instance", [f"{n:>{CW}}" for n in col_names]))
    print(sep)

    totals = [0] * n_configs
    valid  = [0] * n_configs

    for domain, instance, counts in rows:
        print(fmt_row(domain, instance, [fmt_cell(c) for c in counts]))
        for j, v in enumerate(counts):
            if v is not None:
                totals[j] += v
                valid[j]  += 1

    total_cells = [totals[j] if valid[j] == n_inst else None for j in range(n_configs)]
    print(sep)
    print(fmt_row("TOTAL", f"({n_inst} instances)", [fmt_cell(c) for c in total_cells]))
    print()

    # Per-stage pruning summary (only when all four totals are available).
    if all(v is not None for v in total_cells):
        t = total_cells
        stages = [
            ("UP BoolRPG pruning          (col2 vs col1)", t[0] - t[1], t[0]),
            ("C++ grounder vs UP          (col3 vs col1)", t[0] - t[2], t[0]),
            ("Numeric grounding on C++    (col4 vs col3)", t[2] - t[3], t[0]),
            ("C++ + numeric grnd vs UP    (col4 vs col1)", t[0] - t[3], t[0]),
        ]
        for label, pruned, base in stages:
            pct  = 100.0 * pruned / base if base > 0 else 0.0
            sign = "+" if pruned >= 0 else ""
            print(f"  {label}: {sign}{pruned:>8} actions  ({sign}{pct:.1f}%)")


if __name__ == "__main__":
    main()
