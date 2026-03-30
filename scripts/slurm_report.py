#!/usr/bin/env python3
"""
Report generator for RantanPlan SLURM benchmark results.

Generates:
1. Survival (cactus) plot comparing strategies by solving time
2. Pairwise scatter plots between strategies (linear + log scale)
3. CSV summary tables (per-domain, overall, per-instance)

Usage:
    python scripts/slurm_report.py logs/ --stats-dir stats_and_plans/ -o report/
"""
import argparse
import csv
import re
import sys
from dataclasses import dataclass, field
from enum import Enum
from itertools import combinations
from pathlib import Path
from typing import Dict, List, Optional, Tuple

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages


# --- Enums and Data Structures ---

class Status(Enum):
    SOLVED = "solved"
    TIMEOUT = "timeout"
    MEMOUT = "memout"
    ERROR = "error"


@dataclass
class TimestepData:
    timestep: int
    formula_time: float
    solve_time: float
    step_time: float
    memory_mb: int


@dataclass
class SlurmJobResult:
    domain: str
    instance: str
    strategy: str
    mode: str
    log_file: Path
    status: Status
    total_time: Optional[float] = None
    horizon: Optional[int] = None
    num_actions: Optional[int] = None
    last_memory_mb: Optional[int] = None
    timesteps: List[TimestepData] = field(default_factory=list)


@dataclass
class ExperimentData:
    jobs: List[SlurmJobResult]
    timeout: float
    strategies: List[str]
    domains: List[str]
    skipped_empty: int = 0
    skipped_no_metadata: int = 0

    def job_for(
        self, domain: str, instance: str, strategy: str
    ) -> Optional[SlurmJobResult]:
        for j in self.jobs:
            if j.domain == domain and j.instance == instance and j.strategy == strategy:
                return j
        return None

    def instance_keys(self) -> List[Tuple[str, str]]:
        """Return sorted unique (domain, instance) pairs."""
        keys = sorted({(j.domain, j.instance) for j in self.jobs})
        return keys


# --- Regex Patterns ---

RE_TIMESTEP = re.compile(
    r"\[Solving T(\d+)\s*\]\s+"
    r"(?:\[(?:Init|Forward|Backward)\]:\s+\|\s+)?"  # optional double-tail prefix
    r"formula:\s+([\d.]+)s\s+\|\s+"
    r"solve:\s+([\d.]+)s\s+\|\s+"
    r"step:\s+([\d.]+)s\s+\|\s+"
    r"mem:\s+(\d+)MB"
)
RE_TIMESTEP_CAUSAL = re.compile(
    r"\[Solving T(\d+)\s*\]\s+"
    r"solve:\s+([\d.]+)s\s+\|\s+"
    r"round:\s+([\d.]+)s\s+\|\s+"
    r"horizon:\s+\d+\s+\|\s+"
    r"blocked:\s+\d+\s+\|\s+"
    r"total_entries:\s+\d+\s+\|\s+"
    r"mem:\s+(\d+)MB"
)
RE_PLAN_FOUND = re.compile(
    r"\*\*\* PLAN FOUND: horizon=(\d+), actions=(\d+) \(total time: ([\d.]+)s\)"
)
RE_DT_PLAN_FOUND = re.compile(
    r"\*\*\* PLAN FOUND at iteration \d+ \(plan length (\d+), total time: ([\d.]+)s\)"
)
RE_R2E_PLAN_FOUND = re.compile(
    r"\*\*\* PLAN FOUND: (\d+) actions, (\d+) rounds, \d+ extensions \(total time: ([\d.]+)s\)"
)
RE_CAUSAL_EXISTS_PLAN_FOUND = re.compile(
    r"\*\*\* PLAN FOUND: (\d+) actions, (\d+) rounds, horizon=(\d+) \(total time: ([\d.]+)s\)"
)
RE_OPTIMAL_FOUND = re.compile(
    r"\*\*\* OPTIMAL PLAN FOUND: horizon=(\d+), actions=(\d+), cost=[\d.]+.*\(total time: ([\d.]+)s\)"
)
RE_BEST_FOUND = re.compile(
    r"\*\*\* BEST PLAN FOUND: horizon=(\d+), actions=(\d+), cost=[\d.]+.*\(total time: ([\d.]+)s\)"
)
RE_PLANNER_TIMEOUT = re.compile(r"Planner timed out\.")
RE_STATUS = re.compile(r"Status: PlanGenerationResultStatus\.(\w+)")
RE_SLURM_CANCEL = re.compile(r"CANCELLED.*DUE TO TIME LIMIT")
RE_ERROR = re.compile(r"Error during planning:|Aborted \(core dumped\)")
RE_OOM = re.compile(r"slurmstepd.*oom|Killed|Out of memory|std::bad_alloc", re.IGNORECASE)
RE_METADATA = re.compile(r"^(\w+)=(.+)$")


# --- Parsing ---

def parse_slurm_log(log_path: Path) -> Optional[SlurmJobResult]:
    """Parse a single SLURM .out log file. Streams line-by-line for huge files."""
    metadata: Dict[str, str] = {}
    timesteps: List[TimestepData] = []
    solved = False
    total_time: Optional[float] = None
    horizon: Optional[int] = None
    num_actions: Optional[int] = None
    last_memory_mb: Optional[int] = None
    is_timeout = False
    is_memout = False
    is_error = False

    try:
        with open(log_path, "r", errors="replace") as f:
            # Read metadata header (first few lines)
            for line in f:
                line = line.strip()
                m = RE_METADATA.match(line)
                if m and len(metadata) < 4:
                    metadata[m.group(1)] = m.group(2)
                else:
                    # First non-metadata line; process it too
                    break
            else:
                # File exhausted during metadata
                line = ""

            # Check required metadata
            if "domain" not in metadata or "strategy" not in metadata:
                print(f"Warning: skipping {log_path.name} (missing metadata)", file=sys.stderr)
                return None

            # Process the first non-metadata line and continue
            def process_line(line: str) -> None:
                nonlocal solved, total_time, horizon, num_actions
                nonlocal last_memory_mb, is_timeout, is_memout, is_error

                m = RE_TIMESTEP.search(line)
                if m:
                    ts = TimestepData(
                        timestep=int(m.group(1)),
                        formula_time=float(m.group(2)),
                        solve_time=float(m.group(3)),
                        step_time=float(m.group(4)),
                        memory_mb=int(m.group(5)),
                    )
                    timesteps.append(ts)
                    last_memory_mb = ts.memory_mb
                    return

                m = RE_TIMESTEP_CAUSAL.search(line)
                if m:
                    ts = TimestepData(
                        timestep=int(m.group(1)),
                        formula_time=0.0,
                        solve_time=float(m.group(2)),
                        step_time=float(m.group(3)),
                        memory_mb=int(m.group(4)),
                    )
                    timesteps.append(ts)
                    last_memory_mb = ts.memory_mb
                    return

                for pattern in (RE_PLAN_FOUND, RE_OPTIMAL_FOUND, RE_BEST_FOUND):
                    m = pattern.search(line)
                    if m:
                        solved = True
                        horizon = int(m.group(1))
                        num_actions = int(m.group(2))
                        total_time = float(m.group(3))
                        return

                m = RE_DT_PLAN_FOUND.search(line)
                if m:
                    solved = True
                    num_actions = int(m.group(1))
                    total_time = float(m.group(2))
                    return

                m = RE_R2E_PLAN_FOUND.search(line)
                if m:
                    solved = True
                    num_actions = int(m.group(1))
                    total_time = float(m.group(3))
                    return

                m = RE_CAUSAL_EXISTS_PLAN_FOUND.search(line)
                if m:
                    solved = True
                    num_actions = int(m.group(1))
                    horizon = int(m.group(3))
                    total_time = float(m.group(4))
                    return

                if RE_PLANNER_TIMEOUT.search(line):
                    is_timeout = True
                    return
                if RE_SLURM_CANCEL.search(line):
                    is_timeout = True
                    return

                m = RE_STATUS.search(line)
                if m:
                    status_str = m.group(1)
                    if status_str in ("SOLVED_SATISFICING", "SOLVED_OPTIMALLY"):
                        solved = True
                    elif status_str == "TIMEOUT":
                        is_timeout = True
                    elif status_str == "INTERNAL_ERROR":
                        is_error = True
                    return

                if RE_OOM.search(line):
                    is_memout = True
                    return
                if RE_ERROR.search(line):
                    is_error = True
                    return

            if line:
                process_line(line)

            for line in f:
                process_line(line.strip())

    except OSError as e:
        print(f"Warning: could not read {log_path}: {e}", file=sys.stderr)
        return None

    # Determine status with priority: SOLVED > MEMOUT > TIMEOUT > ERROR
    if solved:
        status = Status.SOLVED
    elif is_memout:
        status = Status.MEMOUT
    elif is_timeout:
        status = Status.TIMEOUT
    elif is_error:
        status = Status.ERROR
    else:
        # No clear status detected — likely killed by SLURM without a message
        status = Status.TIMEOUT

    return SlurmJobResult(
        domain=metadata.get("domain", "unknown"),
        instance=metadata.get("instance", "unknown"),
        strategy=metadata.get("strategy", "unknown"),
        mode=metadata.get("mode", "unknown"),
        log_file=log_path,
        status=status,
        total_time=total_time,
        horizon=horizon,
        num_actions=num_actions,
        last_memory_mb=last_memory_mb,
        timesteps=timesteps,
    )


def parse_stats_file(stats_path: Path) -> Dict[str, float]:
    """Parse a key: value stats file."""
    result: Dict[str, float] = {}
    try:
        with open(stats_path, "r") as f:
            for line in f:
                line = line.strip()
                if ": " in line:
                    key, val = line.split(": ", 1)
                    try:
                        result[key.strip()] = float(val.strip())
                    except ValueError:
                        pass
    except OSError:
        pass
    return result


# --- Discovery and Loading ---

def discover_stats_files(
    stats_dir: Path, known_strategies: List[str]
) -> Dict[Tuple[str, str, str, str], Path]:
    """Index stats files by (domain, instance, strategy, mode).

    Strategy names use hyphens, other fields use underscores as separators,
    so we match known strategies from the suffix to parse unambiguously.
    """
    index: Dict[Tuple[str, str, str, str], Path] = {}
    strategies_sorted = sorted(known_strategies, key=len, reverse=True)

    for p in stats_dir.glob("*.stat"):
        stem = p.stem  # e.g. block-grouping_instance_5_20_5_2_r2e_satisficing
        # Mode is always the last _-segment
        last_under = stem.rfind("_")
        if last_under < 0:
            continue
        mode = stem[last_under + 1:]
        rest = stem[:last_under]

        # Find strategy by matching known strategies from the end
        matched_strategy = None
        domain_instance = None
        for strat in strategies_sorted:
            suffix = "_" + strat
            if rest.endswith(suffix):
                matched_strategy = strat
                domain_instance = rest[: len(rest) - len(suffix)]
                break

        if not matched_strategy or not domain_instance:
            continue

        # Split domain_instance — domain is the first segment(s) up to the
        # instance part. We can't split deterministically, so store the whole
        # thing and match by log-derived metadata later.
        index[(domain_instance, matched_strategy, mode)] = p

    # Re-index using domain+instance from log data
    return index


def load_experiment(
    logs_dir: Path,
    stats_dir: Optional[Path],
    timeout_override: Optional[float],
) -> ExperimentData:
    """Load all SLURM logs and optionally enrich with stats."""
    log_files = sorted(logs_dir.glob("*.out"))
    if not log_files:
        print(f"Error: no .out files found in {logs_dir}", file=sys.stderr)
        sys.exit(1)

    print(f"Parsing {len(log_files)} log files...", file=sys.stderr)
    jobs: List[SlurmJobResult] = []
    skipped_empty = 0
    skipped_no_metadata = 0
    for i, lf in enumerate(log_files):
        if (i + 1) % 200 == 0:
            print(f"  parsed {i + 1}/{len(log_files)}...", file=sys.stderr)
        if lf.stat().st_size == 0:
            skipped_empty += 1
            continue
        job = parse_slurm_log(lf)
        if job is not None:
            jobs.append(job)
        else:
            skipped_no_metadata += 1

    print(f"Parsed {len(jobs)} jobs successfully.", file=sys.stderr)
    if skipped_empty:
        print(
            f"Warning: {skipped_empty} empty log files (jobs died before producing output)",
            file=sys.stderr,
        )
    if skipped_no_metadata:
        print(
            f"Warning: {skipped_no_metadata} files skipped (missing metadata)",
            file=sys.stderr,
        )

    # Determine timeout
    if timeout_override is not None:
        timeout = timeout_override
    else:
        solved_times = [j.total_time for j in jobs if j.status == Status.SOLVED and j.total_time is not None]
        if solved_times:
            timeout = max(solved_times) * 1.1
            # Round to a nice number
            timeout = max(timeout, 60.0)
        else:
            timeout = 1800.0
        print(f"Auto-detected timeout: {timeout:.0f}s", file=sys.stderr)

    # Enrich with stats files if available
    if stats_dir and stats_dir.is_dir():
        known_strategies = sorted({j.strategy for j in jobs})
        stats_index = discover_stats_files(stats_dir, known_strategies)

        enriched = 0
        for job in jobs:
            if job.status != Status.SOLVED:
                continue
            # Build the domain_instance key the same way the filename was constructed
            di_key = f"{job.domain}_{job.instance}"
            key = (di_key, job.strategy, job.mode)
            stats_path = stats_index.get(key)
            if stats_path:
                stats = parse_stats_file(stats_path)
                if "planner.total_time" in stats:
                    job.total_time = stats["planner.total_time"]
                if job.horizon is None and "planner.solution_horizon" in stats:
                    job.horizon = int(stats["planner.solution_horizon"])
                if job.num_actions is None and "planner.plan_length" in stats:
                    job.num_actions = int(stats["planner.plan_length"])
                enriched += 1

        print(f"Enriched {enriched} jobs with stats data.", file=sys.stderr)

    strategies = sorted({j.strategy for j in jobs})
    domains = sorted({j.domain for j in jobs})

    return ExperimentData(
        jobs=jobs,
        timeout=timeout,
        strategies=strategies,
        domains=domains,
        skipped_empty=skipped_empty,
        skipped_no_metadata=skipped_no_metadata,
    )


# --- CSV Generation ---

def write_instances_csv(data: ExperimentData, output_path: Path) -> None:
    """Write per-instance detail CSV."""
    with open(output_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            "domain", "instance", "strategy", "mode", "status",
            "total_time", "horizon", "num_actions", "last_memory_mb",
        ])
        for job in sorted(data.jobs, key=lambda j: (j.domain, j.instance, j.strategy)):
            writer.writerow([
                job.domain,
                job.instance,
                job.strategy,
                job.mode,
                job.status.value,
                f"{job.total_time:.3f}" if job.total_time is not None else "",
                job.horizon if job.horizon is not None else "",
                job.num_actions if job.num_actions is not None else "",
                job.last_memory_mb if job.last_memory_mb is not None else "",
            ])
    print(f"Wrote {output_path}", file=sys.stderr)


def write_summary_by_domain_csv(data: ExperimentData, output_path: Path) -> None:
    """Write per-domain summary CSV."""
    with open(output_path, "w", newline="") as f:
        writer = csv.writer(f)
        # Header
        header = ["domain"]
        for s in data.strategies:
            header.extend([f"{s}_solved", f"{s}_timeout", f"{s}_memout", f"{s}_error", f"{s}_total"])
        writer.writerow(header)

        for domain in data.domains:
            row = [domain]
            for strategy in data.strategies:
                domain_jobs = [
                    j for j in data.jobs
                    if j.domain == domain and j.strategy == strategy
                ]
                solved = sum(1 for j in domain_jobs if j.status == Status.SOLVED)
                timeout = sum(1 for j in domain_jobs if j.status == Status.TIMEOUT)
                memout = sum(1 for j in domain_jobs if j.status == Status.MEMOUT)
                error = sum(1 for j in domain_jobs if j.status == Status.ERROR)
                row.extend([solved, timeout, memout, error, len(domain_jobs)])
            writer.writerow(row)
    print(f"Wrote {output_path}", file=sys.stderr)


def write_summary_overall_csv(data: ExperimentData, output_path: Path) -> None:
    """Write overall summary CSV."""
    with open(output_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["strategy", "solved", "timeout", "memout", "error", "total"])
        for strategy in data.strategies:
            strat_jobs = [j for j in data.jobs if j.strategy == strategy]
            solved = sum(1 for j in strat_jobs if j.status == Status.SOLVED)
            timeout = sum(1 for j in strat_jobs if j.status == Status.TIMEOUT)
            memout = sum(1 for j in strat_jobs if j.status == Status.MEMOUT)
            error = sum(1 for j in strat_jobs if j.status == Status.ERROR)
            writer.writerow([strategy, solved, timeout, memout, error, len(strat_jobs)])
        skipped = data.skipped_empty + data.skipped_no_metadata
        if skipped:
            writer.writerow([f"(unparseable: {skipped} log files)", "", "", "", "", skipped])
    print(f"Wrote {output_path}", file=sys.stderr)


# --- Plotting ---

MARKERS = ["o", "s", "^", "D", "v", "P", "X", "*", "p", "h", "<", ">", "d", "H", "8"]


def get_strategy_colors(strategies: List[str]) -> Dict[str, object]:
    """Assign consistent colors to strategies."""
    cmap = matplotlib.colormaps["tab10"]
    return {s: cmap(i % 10) for i, s in enumerate(strategies)}


def get_domain_styles(domains: List[str]) -> Dict[str, Tuple[str, object]]:
    """Assign (marker, color) pairs to domains."""
    cmap = matplotlib.colormaps["tab20"]
    styles: Dict[str, Tuple[str, object]] = {}
    for i, d in enumerate(domains):
        marker = MARKERS[i % len(MARKERS)]
        color = cmap(i % 20)
        styles[d] = (marker, color)
    return styles


def plot_cactus(data: ExperimentData, output_path: Path) -> None:
    """Generate survival/cactus plot."""
    colors = get_strategy_colors(data.strategies)

    fig, ax = plt.subplots(figsize=(12, 7))

    instance_keys = data.instance_keys()
    num_instances = len(instance_keys)

    for strategy in data.strategies:
        # Collect solving times; unsolved instances get the timeout value
        times: List[float] = []
        for domain, instance in instance_keys:
            job = data.job_for(domain, instance, strategy)
            if job is not None and job.status == Status.SOLVED and job.total_time is not None:
                times.append(job.total_time)
            else:
                times.append(data.timeout)
        times.sort()
        x = list(range(1, len(times) + 1))
        ax.plot(x, times, marker="o", markersize=4, label=strategy, color=colors[strategy])

    ax.axhline(
        y=data.timeout, color="gray", linestyle="--", linewidth=0.8,
        label=f"timeout ({data.timeout:.0f}s)",
    )
    ax.set_xlabel("Number of instances solved")
    ax.set_ylabel("Time (s)")
    ax.set_yscale("log")
    ax.set_title("Survival Plot")
    ax.legend(fontsize=9, loc="best")
    ax.grid(True, which="both", alpha=0.3)
    fig.tight_layout()

    fig.savefig(str(output_path))
    plt.close(fig)
    print(f"Wrote {output_path}", file=sys.stderr)


def plot_scatter_pair(
    data: ExperimentData,
    strategy_a: str,
    strategy_b: str,
    ax: plt.Axes,
    log_scale: bool,
    timeout: float,
    domain_styles: Dict[str, Tuple[str, object]],
) -> None:
    """Plot a single scatter comparison on the given axes."""
    instance_keys = data.instance_keys()

    plotted_domains: Dict[str, bool] = {}

    for domain, instance in instance_keys:
        job_a = data.job_for(domain, instance, strategy_a)
        job_b = data.job_for(domain, instance, strategy_b)
        if job_a is None or job_b is None:
            continue

        time_a = job_a.total_time if job_a.status == Status.SOLVED and job_a.total_time is not None else timeout
        time_b = job_b.total_time if job_b.status == Status.SOLVED and job_b.total_time is not None else timeout

        marker, color = domain_styles.get(domain, ("o", "gray"))
        label = domain if domain not in plotted_domains else None
        plotted_domains[domain] = True

        ax.scatter(
            time_a, time_b,
            marker=marker, color=color, s=30, alpha=0.7,
            label=label, edgecolors="none",
        )

    # Diagonal reference line
    if log_scale:
        lo = 0.01
    else:
        lo = 0
    hi = timeout * 1.1
    ax.plot([lo, hi], [lo, hi], "k--", linewidth=0.5, alpha=0.5)

    # Timeout reference lines
    ax.axhline(y=timeout, color="gray", linestyle=":", linewidth=0.5, alpha=0.5)
    ax.axvline(x=timeout, color="gray", linestyle=":", linewidth=0.5, alpha=0.5)

    ax.set_xlabel(strategy_a)
    ax.set_ylabel(strategy_b)

    if log_scale:
        ax.set_xscale("log")
        ax.set_yscale("log")
        ax.set_title(f"{strategy_a} vs {strategy_b} (log)")
    else:
        ax.set_title(f"{strategy_a} vs {strategy_b} (linear)")

    ax.set_xlim(left=lo if log_scale else -timeout * 0.02, right=hi)
    ax.set_ylim(bottom=lo if log_scale else -timeout * 0.02, top=hi)
    ax.set_aspect("equal", adjustable="box")
    ax.grid(True, which="both", alpha=0.3)


def plot_all_scatters(data: ExperimentData, output_dir: Path) -> None:
    """Generate all pairwise scatter plots."""
    domain_styles = get_domain_styles(data.domains)
    pairs = list(combinations(data.strategies, 2))

    for strategy_a, strategy_b in pairs:
        fig, (ax_lin, ax_log) = plt.subplots(1, 2, figsize=(16, 7))

        plot_scatter_pair(data, strategy_a, strategy_b, ax_lin, False, data.timeout, domain_styles)
        plot_scatter_pair(data, strategy_a, strategy_b, ax_log, True, data.timeout, domain_styles)

        # Shared legend from log axis
        handles, labels = ax_log.get_legend_handles_labels()
        if handles:
            fig.legend(
                handles, labels,
                loc="center right",
                fontsize=7,
                bbox_to_anchor=(1.0, 0.5),
                ncol=1,
            )

        fig.suptitle(f"{strategy_a} vs {strategy_b}", fontsize=13)
        fig.tight_layout(rect=[0, 0, 0.85, 0.95])

        safe_a = strategy_a.replace("/", "_")
        safe_b = strategy_b.replace("/", "_")
        path = output_dir / f"scatter_{safe_a}_vs_{safe_b}.pdf"
        fig.savefig(str(path))
        plt.close(fig)
        print(f"Wrote {path}", file=sys.stderr)


# --- Main ---

def generate_all_outputs(data: ExperimentData, output_dir: Path) -> None:
    """Generate all output files."""
    output_dir.mkdir(parents=True, exist_ok=True)

    # CSVs
    write_instances_csv(data, output_dir / "instances.csv")
    write_summary_by_domain_csv(data, output_dir / "summary_by_domain.csv")
    write_summary_overall_csv(data, output_dir / "summary_overall.csv")

    # Plots
    plot_cactus(data, output_dir / "cactus.pdf")
    plot_all_scatters(data, output_dir)

    # Print summary
    print("\n=== Summary ===", file=sys.stderr)
    for strategy in data.strategies:
        strat_jobs = [j for j in data.jobs if j.strategy == strategy]
        solved = sum(1 for j in strat_jobs if j.status == Status.SOLVED)
        timeout = sum(1 for j in strat_jobs if j.status == Status.TIMEOUT)
        memout = sum(1 for j in strat_jobs if j.status == Status.MEMOUT)
        error = sum(1 for j in strat_jobs if j.status == Status.ERROR)
        print(
            f"  {strategy}: {solved} solved, {timeout} timeout, "
            f"{memout} memout, {error} error (total: {len(strat_jobs)})",
            file=sys.stderr,
        )
    skipped = data.skipped_empty + data.skipped_no_metadata
    if skipped:
        print(
            f"\n  Note: {skipped} log files could not be parsed "
            f"({data.skipped_empty} empty, {data.skipped_no_metadata} missing metadata). "
            f"These jobs are not included in any strategy totals.",
            file=sys.stderr,
        )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Analyze SLURM benchmark results from RantanPlan experiments.",
    )
    parser.add_argument(
        "logs_dir",
        help="Directory with SLURM .out log files",
    )
    parser.add_argument(
        "--stats-dir",
        default=None,
        help="Directory with stats/plan files (optional, for enriched timing data)",
    )
    parser.add_argument(
        "-o", "--output-dir",
        default="slurm_report",
        help="Output directory for generated files (default: slurm_report)",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=None,
        help="Override timeout value in seconds (auto-detected if not set)",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    logs_dir = Path(args.logs_dir)
    if not logs_dir.is_dir():
        print(f"Error: {logs_dir} is not a directory", file=sys.stderr)
        sys.exit(1)

    stats_dir = Path(args.stats_dir) if args.stats_dir else None
    if stats_dir and not stats_dir.is_dir():
        print(f"Error: {stats_dir} is not a directory", file=sys.stderr)
        sys.exit(1)

    data = load_experiment(logs_dir, stats_dir, args.timeout)
    generate_all_outputs(data, Path(args.output_dir))


if __name__ == "__main__":
    main()
