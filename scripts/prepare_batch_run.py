#!/usr/bin/env python3
"""
Generate a text file of shell commands for SLURM batch execution.

Each line covers one (domain, instance, strategy, mode) combination.
The command echoes run parameters in config-style to stdout before invoking
the planner, so SLURM job output files are self-describing.

Usage:
    python3 prepare_batch_run.py pddl/bench/ -o jobs.txt --output-dir results/
"""
import argparse
import os
from pathlib import Path
from collections import defaultdict

# ---------------------------------------------------------------------------
# Embedded defaults
# ---------------------------------------------------------------------------

DEFAULT_STRATEGIES = [
    "exists",
    "r2e",
    "exists-lazy-semantic-chain",
    "exists-lazy-semantic-chain-dt",
]

DEFAULT_MODES = ["satisficing"]

HORIZON = ["linear", "arithmetic", "geometric", "doubling"]

COMMAND_TEMPLATE = (
    "source .venv/bin/activate && "
    "rantanplan -d {domain} -p {instance} "
    "--strategy {strategy} --mode {mode} "
    "--timeout {timeout} "
    "--horizon-schedule {horizon_schedule} "
    "--output-plan {planfile} --stats-file {statsfile}"
)

# ---------------------------------------------------------------------------
# PDDL discovery (mirrors benchmark.py logic)
# ---------------------------------------------------------------------------

def discover_pddl_instances(root_dir):
    """Discover all PDDL domain/problem pairs under root_dir.

    Returns dict mapping domain name -> list of (domain_file, problem_file).
    """
    instances_by_domain = defaultdict(list)
    root_path = Path(root_dir)

    for domain_dir in sorted(root_path.iterdir()):
        if not domain_dir.is_dir():
            continue

        domain_file = domain_dir / "domain.pddl"
        instances_dir = domain_dir / "instances"

        if domain_file.exists() and instances_dir.exists() and instances_dir.is_dir():
            for instance_file in sorted(instances_dir.glob("*.pddl")):
                instances_by_domain[domain_dir.name].append(
                    (domain_file, instance_file)
                )

    # Fallback: walk for domain*/problem* pairs
    if not instances_by_domain:
        for dirpath, _, filenames in os.walk(root_dir):
            domain_files = []
            problem_files = []
            for filename in sorted(filenames):
                if filename.endswith(".pddl"):
                    if "domain" in filename.lower():
                        domain_files.append(Path(dirpath) / filename)
                    elif "problem" in filename.lower():
                        problem_files.append(Path(dirpath) / filename)

            if len(domain_files) == 1 and problem_files:
                domain_name = Path(dirpath).name
                for problem_file in problem_files:
                    instances_by_domain[domain_name].append(
                        (domain_files[0], problem_file)
                    )

    return instances_by_domain

# ---------------------------------------------------------------------------
# Command generation
# ---------------------------------------------------------------------------

def make_echo_header(domain, instance, strategy, mode):
    """Build the echo prefix that prints run parameters to stdout."""
    lines = [
        f"domain={domain}",
        f"instance={instance}",
        f"strategy={strategy}",
        f"mode={mode}",
    ]
    return " && ".join(f"echo '{line}'" for line in lines)


def generate_commands(instances_by_domain, strategies, modes, output_dir, timeout, horizon_schedules):
    """Yield one shell command per (domain, instance, strategy, mode) combo."""
    for domain_name, pairs in sorted(instances_by_domain.items()):
        for domain_file, instance_file in pairs:
            instance_name = instance_file.stem
            for strategy in strategies:
                for mode in modes:
                    for horizon_schedule in horizon_schedules:
                        prefix = f"{domain_name}_{instance_name}_{strategy}_{mode}_{horizon_schedule}"
                        planfile = os.path.join(output_dir, f"{prefix}.plan")
                        statsfile = os.path.join(output_dir, f"{prefix}.stat")

                        echo = make_echo_header(domain_name, instance_name, strategy, mode)
                        cmd = COMMAND_TEMPLATE.format(
                            domain=domain_file,
                            instance=instance_file,
                            strategy=strategy,
                            mode=mode,
                            timeout=timeout,
                            horizon_schedule=horizon_schedule,
                            planfile=planfile,
                            statsfile=statsfile,
                        )
                        yield f"{echo} && {cmd}"

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Generate SLURM batch commands for rantanplan experiments."
    )
    parser.add_argument(
        "pddl_dir",
        help="Path to benchmark directory (e.g. pddl/bench/)",
    )
    parser.add_argument(
        "-o", "--output-file",
        required=True,
        help="Text file to append commands to",
    )
    parser.add_argument(
        "--output-dir",
        required=True,
        help="Base directory for plan and stats output files",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=0,
        help=f"Timeout per run in seconds (default: 0)",
    )
    parser.add_argument(
        "--horizon-schedule",
        nargs="+",
        default=HORIZON,
        help="Horizon schedules to use (default: linear)",
    )
    parser.add_argument(
        "--strategies",
        nargs="+",
        default=DEFAULT_STRATEGIES,
        help="Strategies to benchmark (default: all known strategies)",
    )
    parser.add_argument(
        "--modes",
        nargs="+",
        default=DEFAULT_MODES,
        help="Planner modes to use (default: satisficing optimal anytime)",
    )

    args = parser.parse_args()

    instances_by_domain = discover_pddl_instances(args.pddl_dir)
    if not instances_by_domain:
        print(f"No PDDL instances found in {args.pddl_dir}")
        return 1

    total_instances = sum(len(v) for v in instances_by_domain.values())
    total_commands = total_instances * len(args.strategies) * len(args.modes) * len(args.horizon_schedule)

    print(f"Domains: {len(instances_by_domain)}")
    print(f"Instances: {total_instances}")
    print(f"Strategies: {len(args.strategies)}")
    print(f"Modes: {len(args.modes)}")
    print(f"Horizon schedules: {len(args.horizon_schedule)}")
    print(f"Total commands: {total_commands}")

    count = 0
    with open(args.output_file, "a") as f:
        for cmd in generate_commands(
            instances_by_domain, args.strategies, args.modes, args.output_dir, args.timeout, args.horizon_schedule
        ):
            f.write(cmd + "\n")
            count += 1

    print(f"Appended {count} commands to {args.output_file}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
