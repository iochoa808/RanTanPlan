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
from typing import Dict, List, Optional, Set, Tuple

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
    stats: Dict[str, float] = field(default_factory=dict)


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
) -> Dict[Tuple[str, str, str], Path]:
    """Index stats files by (domain_instance, strategy, mode).

    Filenames: {domain}_{instance}_{strategy}_{mode}_{horizon_schedule}.stat
    Strategy names use hyphens; other fields use underscores as separators.
    """
    index: Dict[Tuple[str, str, str], Path] = {}
    strategies_sorted = sorted(known_strategies, key=len, reverse=True)

    for p in stats_dir.glob("*.stat"):
        stem = p.stem
        # Strip last two _-separated segments: horizon_schedule and mode
        parts = stem.rsplit("_", 2)
        if len(parts) == 3:
            rest, mode, _horizon = parts
        elif len(parts) == 2:
            rest, mode = parts
        else:
            continue

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

        index[(domain_instance, matched_strategy, mode)] = p

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
                job.stats = stats
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


# --- Stats Analysis: Discovery ---


def get_stat(job: SlurmJobResult, key: str, default: float = 0.0) -> float:
    """Get a stat value, defaulting to 0 for missing Z3 counters."""
    return job.stats.get(key, default)


def discover_stat_prefixes(data: ExperimentData) -> Dict[str, Set[str]]:
    """Map stat key prefix -> set of strategies that have keys with that prefix."""
    result: Dict[str, Set[str]] = {}
    for job in data.jobs:
        if not job.stats:
            continue
        for key in job.stats:
            prefix = key.split(".")[0]
            if prefix not in result:
                result[prefix] = set()
            result[prefix].add(job.strategy)
    return result


def discover_strategy_pairs(
    strategies: List[str],
) -> List[Tuple[str, str, str]]:
    """Find pairs where one strategy name is a dash-suffix extension of another.
    Returns [(base, extended, suffix), ...]."""
    pairs: List[Tuple[str, str, str]] = []
    sorted_strats = sorted(strategies, key=len)
    for i, base in enumerate(sorted_strats):
        for extended in sorted_strats[i + 1 :]:
            if extended.startswith(base + "-"):
                suffix = extended[len(base) + 1 :]
                pairs.append((base, extended, suffix))
    return pairs


def get_common_solved(
    data: ExperimentData, strategy_a: str, strategy_b: str
) -> List[Tuple[str, str, SlurmJobResult, SlurmJobResult]]:
    """Return [(domain, instance, job_a, job_b)] for instances solved by both with stats."""
    results: List[Tuple[str, str, SlurmJobResult, SlurmJobResult]] = []
    for domain, instance in data.instance_keys():
        job_a = data.job_for(domain, instance, strategy_a)
        job_b = data.job_for(domain, instance, strategy_b)
        if (
            job_a is not None
            and job_b is not None
            and job_a.status == Status.SOLVED
            and job_b.status == Status.SOLVED
            and job_a.stats
            and job_b.stats
        ):
            results.append((domain, instance, job_a, job_b))
    return results


# --- Stats Analysis: CSV Generation ---


def write_stats_full_csv(data: ExperimentData, output_path: Path) -> None:
    """Write complete stats dump: one row per solved job with stats."""
    all_keys = sorted({k for job in data.jobs for k in job.stats})
    if not all_keys:
        return
    with open(output_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(
            ["domain", "instance", "strategy", "mode", "status", "total_time"]
            + all_keys
        )
        for job in sorted(
            data.jobs, key=lambda j: (j.domain, j.instance, j.strategy)
        ):
            if not job.stats:
                continue
            row = [
                job.domain,
                job.instance,
                job.strategy,
                job.mode,
                job.status.value,
                f"{job.total_time:.3f}" if job.total_time is not None else "",
            ]
            for key in all_keys:
                val = job.stats.get(key)
                row.append(f"{val:.6g}" if val is not None else "")
            writer.writerow(row)
    print(f"Wrote {output_path}", file=sys.stderr)


def write_z3_comparison_csv(data: ExperimentData, output_path: Path) -> None:
    """Write Z3 metrics side-by-side for commonly-solved instances."""
    z3_keys = sorted(
        {k for job in data.jobs for k in job.stats if k.startswith("z3.")}
    )
    if not z3_keys:
        return
    with open(output_path, "w", newline="") as f:
        writer = csv.writer(f)
        header = ["domain", "instance"]
        for s in data.strategies:
            for k in z3_keys:
                header.append(f"{s}|{k}")
        writer.writerow(header)

        for domain, instance in data.instance_keys():
            jobs: Dict[str, SlurmJobResult] = {}
            for s in data.strategies:
                j = data.job_for(domain, instance, s)
                if j and j.status == Status.SOLVED and j.stats:
                    jobs[s] = j
            if len(jobs) < 2:
                continue
            row: List[str] = [domain, instance]
            for s in data.strategies:
                j = jobs.get(s)
                for k in z3_keys:
                    if j:
                        val = j.stats.get(k)
                        row.append(f"{val:.6g}" if val is not None else "")
                    else:
                        row.append("")
            writer.writerow(row)
    print(f"Wrote {output_path}", file=sys.stderr)


def write_propagator_impact_csv(data: ExperimentData, output_path: Path) -> None:
    """Write propagator impact analysis for auto-discovered strategy pairs."""
    pairs = discover_strategy_pairs(data.strategies)
    if not pairs:
        return
    prefixes = discover_stat_prefixes(data)

    ratio_keys = [
        "planner.total_time",
        "planner.solve_time",
        "z3.time",
        "z3.decisions",
        "z3.conflicts",
        "z3.propagations",
        "z3.restarts",
        "z3.rlimit count",
        "z3.mk bool var",
        "z3.mk clause",
    ]

    # Collect propagator-specific keys (present in extended but not base)
    prop_keys: Set[str] = set()
    for base, extended, _ in pairs:
        for job in data.jobs:
            if job.strategy == extended and job.stats:
                for k in job.stats:
                    prefix = k.split(".")[0]
                    strats_with = prefixes.get(prefix, set())
                    if extended in strats_with and base not in strats_with:
                        prop_keys.add(k)
    prop_keys_sorted = sorted(prop_keys)

    with open(output_path, "w", newline="") as f:
        writer = csv.writer(f)
        header = ["domain", "instance", "base", "extended", "suffix"]
        for k in ratio_keys:
            header.extend([f"base|{k}", f"extended|{k}", f"ratio|{k}"])
        header.extend([f"extended|{k}" for k in prop_keys_sorted])
        writer.writerow(header)

        for base, extended, suffix in pairs:
            common = get_common_solved(data, base, extended)
            for domain, instance, job_base, job_ext in common:
                row: List[str] = [domain, instance, base, extended, suffix]
                for k in ratio_keys:
                    vb = job_base.stats.get(k)
                    ve = job_ext.stats.get(k)
                    row.append(f"{vb:.6g}" if vb is not None else "")
                    row.append(f"{ve:.6g}" if ve is not None else "")
                    if vb is not None and ve is not None and vb > 0:
                        row.append(f"{ve / vb:.4f}")
                    else:
                        row.append("")
                for k in prop_keys_sorted:
                    ve = job_ext.stats.get(k)
                    row.append(f"{ve:.6g}" if ve is not None else "")
                writer.writerow(row)
    print(f"Wrote {output_path}", file=sys.stderr)


def write_domain_aggregate_csv(data: ExperimentData, output_path: Path) -> None:
    """Write per-domain aggregated Z3 metrics."""
    agg_keys = [
        "planner.total_time",
        "planner.solve_time",
        "z3.time",
        "z3.decisions",
        "z3.conflicts",
        "z3.propagations",
        "z3.restarts",
        "z3.rlimit count",
        "z3.max memory",
    ]
    with open(output_path, "w", newline="") as f:
        writer = csv.writer(f)
        header = ["domain", "strategy", "n_solved"]
        for k in agg_keys:
            header.extend([f"median|{k}", f"mean|{k}"])
        writer.writerow(header)

        for domain in data.domains:
            for strategy in data.strategies:
                jobs = [
                    j
                    for j in data.jobs
                    if j.domain == domain
                    and j.strategy == strategy
                    and j.status == Status.SOLVED
                    and j.stats
                ]
                if not jobs:
                    continue
                row: List[str] = [domain, strategy, str(len(jobs))]
                for k in agg_keys:
                    vals = [j.stats[k] for j in jobs if k in j.stats]
                    if vals:
                        vals_sorted = sorted(vals)
                        med = vals_sorted[len(vals) // 2]
                        mean = sum(vals) / len(vals)
                        row.extend([f"{med:.6g}", f"{mean:.6g}"])
                    else:
                        row.extend(["", ""])
                writer.writerow(row)
    print(f"Wrote {output_path}", file=sys.stderr)


# --- Stats Analysis: Plots ---

Z3_SCATTER_METRICS = [
    ("z3.decisions", "Decisions"),
    ("z3.conflicts", "Conflicts"),
    ("z3.propagations", "Propagations"),
    ("z3.rlimit count", "RLimit Count"),
    ("z3.time", "Z3 Time (s)"),
]

ENCODING_METRICS = [
    ("z3.mk bool var", "Boolean Variables"),
    ("z3.mk clause", "Clauses (non-binary)"),
    ("z3.arith-max-columns", "LP Columns"),
    ("z3.arith-max-rows", "LP Rows"),
]

TIME_COMPONENTS = [
    ("z3.time", "Z3 solving", 1.0),
    ("achievers_analysis.total_time_seconds", "Achiever analysis", 1.0),
    ("pass.grounding.time_ms", "Grounding", 0.001),
    ("pass.interference.time_ms", "Interference", 0.001),
    ("pass.numeric-rpg.time_ms", "Numeric RPG", 0.001),
]

DOMAIN_SUMMARY_METRICS = [
    ("planner.total_time", "Total Time (s)"),
    ("z3.decisions", "Z3 Decisions"),
    ("z3.conflicts", "Z3 Conflicts"),
    ("z3.max memory", "Z3 Memory (MB)"),
    ("z3.mk bool var", "Boolean Vars"),
]


def _plot_metric_scatter(
    ax: plt.Axes,
    common: List[Tuple[str, str, SlurmJobResult, SlurmJobResult]],
    metric_key: str,
    title: str,
    xlabel: str,
    ylabel: str,
    domain_styles: Dict[str, Tuple[str, object]],
) -> bool:
    """Plot a log-scale scatter comparing a metric between two sets of jobs.
    Returns True if any points were plotted."""
    plotted_domains: Dict[str, bool] = {}
    all_vals: List[float] = []

    for domain, instance, job_a, job_b in common:
        va = job_a.stats.get(metric_key, 0)
        vb = job_b.stats.get(metric_key, 0)
        if va <= 0 or vb <= 0:
            continue
        all_vals.extend([va, vb])
        marker, color = domain_styles.get(domain, ("o", "gray"))
        label = domain if domain not in plotted_domains else None
        plotted_domains[domain] = True
        ax.scatter(
            va, vb,
            marker=marker, color=color, s=25, alpha=0.7,
            label=label, edgecolors="none",
        )

    if not all_vals:
        ax.set_visible(False)
        return False

    lo = max(min(all_vals) * 0.5, 1e-4)
    hi = max(all_vals) * 2
    ax.plot([lo, hi], [lo, hi], "k--", linewidth=0.5, alpha=0.5)
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.set_xlim(lo, hi)
    ax.set_ylim(lo, hi)
    ax.set_aspect("equal", adjustable="box")
    ax.grid(True, which="both", alpha=0.3)
    return True


def plot_z3_metric_scatters(data: ExperimentData, pdf: PdfPages) -> None:
    """Z3 metric scatter plots for each strategy pair."""
    domain_styles = get_domain_styles(data.domains)
    pairs = list(combinations(data.strategies, 2))

    for strategy_a, strategy_b in pairs:
        common = get_common_solved(data, strategy_a, strategy_b)
        if len(common) < 2:
            continue

        n_metrics = len(Z3_SCATTER_METRICS)
        ncols = 3
        nrows = (n_metrics + ncols - 1) // ncols
        fig, axes = plt.subplots(
            nrows, ncols, figsize=(6 * ncols, 5.5 * nrows), squeeze=False
        )
        axes_flat = axes.flatten()

        any_plotted = False
        for idx, (metric_key, metric_label) in enumerate(Z3_SCATTER_METRICS):
            if _plot_metric_scatter(
                axes_flat[idx], common, metric_key, metric_label,
                strategy_a, strategy_b, domain_styles,
            ):
                any_plotted = True

        for idx in range(n_metrics, len(axes_flat)):
            axes_flat[idx].set_visible(False)

        if not any_plotted:
            plt.close(fig)
            continue

        handles, labels = axes_flat[0].get_legend_handles_labels()
        if handles:
            fig.legend(
                handles, labels, loc="lower right", fontsize=7,
                bbox_to_anchor=(0.98, 0.02),
                ncol=max(1, len(labels) // 15 + 1),
            )

        fig.suptitle(f"Z3 Metrics: {strategy_a} vs {strategy_b}", fontsize=13)
        fig.tight_layout(rect=[0, 0.03, 1, 0.95])
        pdf.savefig(fig)
        plt.close(fig)


def plot_propagator_impact(data: ExperimentData, pdf: PdfPages) -> None:
    """Propagator impact analysis for auto-discovered strategy pairs."""
    pairs = discover_strategy_pairs(data.strategies)
    if not pairs:
        return
    prefixes = discover_stat_prefixes(data)
    domain_styles = get_domain_styles(data.domains)

    for base, extended, suffix in pairs:
        common = get_common_solved(data, base, extended)
        if len(common) < 2:
            continue

        # --- Page: Speedup bar chart ---
        ratios: List[float] = []
        bar_labels: List[str] = []
        bar_colors: List[str] = []

        for domain, instance, job_base, job_ext in common:
            tb = job_base.stats.get("planner.total_time")
            te = job_ext.stats.get("planner.total_time")
            if tb and te and tb > 0:
                ratio = te / tb
                ratios.append(ratio)
                bar_labels.append(f"{domain}/{instance}")
                bar_colors.append("tab:green" if ratio < 1 else "tab:red")

        if ratios:
            sorted_data = sorted(zip(ratios, bar_labels, bar_colors))
            ratios_s, labels_s, colors_s = zip(*sorted_data)

            fig, ax = plt.subplots(figsize=(max(10, len(ratios_s) * 0.25), 6))
            x = range(len(ratios_s))
            ax.bar(x, ratios_s, color=colors_s, alpha=0.7, width=0.8)
            ax.axhline(y=1.0, color="black", linewidth=0.8, linestyle="--")
            ax.set_ylabel(f"Time ratio ({extended} / {base})")
            ax.set_title(
                f"Impact of -{suffix}: green = faster, red = slower\n"
                f"({len(ratios_s)} commonly-solved instances)"
            )
            ax.set_xticks(list(x))
            ax.set_xticklabels(labels_s, rotation=90, fontsize=5)
            fig.tight_layout()
            pdf.savefig(fig)
            plt.close(fig)

        # --- Page: Z3 delta scatters ---
        delta_metrics = [
            ("z3.decisions", "Decisions"),
            ("z3.conflicts", "Conflicts"),
            ("z3.propagations", "Propagations"),
        ]
        n_delta = len(delta_metrics)
        fig, axes = plt.subplots(
            1, n_delta, figsize=(6 * n_delta, 5.5), squeeze=False
        )
        axes_flat = axes.flatten()
        any_plotted = False
        for idx, (metric_key, metric_label) in enumerate(delta_metrics):
            if _plot_metric_scatter(
                axes_flat[idx], common, metric_key,
                metric_label, base, extended, domain_styles,
            ):
                any_plotted = True

        if any_plotted:
            handles, labels = axes_flat[0].get_legend_handles_labels()
            if handles:
                fig.legend(
                    handles, labels, loc="center right", fontsize=7,
                    bbox_to_anchor=(1.0, 0.5), ncol=1,
                )
            fig.suptitle(f"Z3 Impact: {base} vs {extended}", fontsize=13)
            fig.tight_layout(rect=[0, 0, 0.88, 0.95])
            pdf.savefig(fig)
        plt.close(fig)

        # --- Page: Propagator-specific stats box plots ---
        prop_prefixes = []
        for prefix, strats in prefixes.items():
            if extended in strats and base not in strats:
                prop_prefixes.append(prefix)

        if not prop_prefixes:
            continue

        prop_keys = sorted(
            {
                k
                for job in data.jobs
                if job.strategy == extended and job.stats
                for k in job.stats
                if k.split(".")[0] in prop_prefixes
            }
        )
        if not prop_keys:
            continue

        domain_groups: Dict[str, List[SlurmJobResult]] = {}
        for domain, instance, _, job_ext in common:
            domain_groups.setdefault(domain, []).append(job_ext)
        domain_names = sorted(domain_groups.keys())

        ncols = min(3, len(prop_keys))
        nrows = (len(prop_keys) + ncols - 1) // ncols
        fig, axes_grid = plt.subplots(
            nrows, ncols, figsize=(6 * ncols, 5 * nrows), squeeze=False
        )
        axes_flat = axes_grid.flatten()

        for idx, key in enumerate(prop_keys):
            ax = axes_flat[idx]
            box_data: List[List[float]] = []
            box_labels: List[str] = []
            for d in domain_names:
                vals = [j.stats.get(key, 0) for j in domain_groups[d]]
                if any(v > 0 for v in vals):
                    box_data.append(vals)
                    box_labels.append(d)
            if box_data:
                ax.boxplot(box_data)
                ax.set_xticklabels(box_labels, rotation=45, ha="right", fontsize=7)
            short_key = key.split(".", 1)[1] if "." in key else key
            ax.set_title(short_key)
            ax.grid(True, axis="y", alpha=0.3)

        for idx in range(len(prop_keys), len(axes_flat)):
            axes_flat[idx].set_visible(False)

        fig.suptitle(
            f"Propagator Stats: {extended} ({', '.join(prop_prefixes)})",
            fontsize=13,
        )
        fig.tight_layout(rect=[0, 0, 1, 0.95])
        pdf.savefig(fig)
        plt.close(fig)


def plot_time_breakdowns(data: ExperimentData, pdf: PdfPages) -> None:
    """Time breakdown stacked bars per strategy."""
    for strategy in data.strategies:
        strat_jobs = [
            j
            for j in data.jobs
            if j.strategy == strategy and j.status == Status.SOLVED and j.stats
        ]
        if not strat_jobs:
            continue

        domain_groups: Dict[str, List[SlurmJobResult]] = {}
        for j in strat_jobs:
            domain_groups.setdefault(j.domain, []).append(j)
        domains = sorted(domain_groups.keys())

        # Determine which time components are non-zero for this strategy
        active_components: List[Tuple[str, str, float]] = []
        for key, label, scale in TIME_COMPONENTS:
            has_any = False
            for d in domains:
                vals = [j.stats.get(key, 0) * scale for j in domain_groups[d]]
                if sum(vals) / len(vals) > 0:
                    has_any = True
                    break
            if has_any:
                active_components.append((key, label, scale))

        component_means: Dict[str, List[float]] = {d: [] for d in domains}
        component_names: List[str] = []
        for key, label, scale in active_components:
            component_names.append(label)
            for d in domains:
                vals = [j.stats.get(key, 0) * scale for j in domain_groups[d]]
                component_means[d].append(sum(vals) / len(vals))

        # Add "Other" = total - accounted
        component_names.append("Other")
        for d in domains:
            total_vals = [
                j.stats.get("planner.total_time", 0) for j in domain_groups[d]
            ]
            mean_total = sum(total_vals) / len(total_vals)
            accounted = sum(component_means[d])
            component_means[d].append(max(0, mean_total - accounted))

        fig, ax = plt.subplots(figsize=(max(10, len(domains) * 0.8), 7))
        x = list(range(len(domains)))
        bottom = [0.0] * len(domains)
        cmap = matplotlib.colormaps["Set2"]

        for i, name in enumerate(component_names):
            vals = [component_means[d][i] for d in domains]
            color = cmap(i % 8)
            ax.bar(x, vals, bottom=bottom, label=name, color=color, alpha=0.85)
            bottom = [b + v for b, v in zip(bottom, vals)]

        ax.set_xticks(x)
        ax.set_xticklabels(domains, rotation=45, ha="right", fontsize=8)
        ax.set_ylabel("Mean time (s)")
        ax.set_title(f"Time Breakdown: {strategy}")
        ax.legend(fontsize=8, loc="best")
        ax.grid(True, axis="y", alpha=0.3)
        fig.tight_layout()
        pdf.savefig(fig)
        plt.close(fig)


def plot_encoding_comparison(data: ExperimentData, pdf: PdfPages) -> None:
    """Encoding size scatter plots across strategy pairs."""
    domain_styles = get_domain_styles(data.domains)
    pairs = list(combinations(data.strategies, 2))

    for strategy_a, strategy_b in pairs:
        common = get_common_solved(data, strategy_a, strategy_b)
        if len(common) < 2:
            continue

        n_metrics = len(ENCODING_METRICS)
        ncols = min(n_metrics, 2)
        nrows = (n_metrics + ncols - 1) // ncols
        fig, axes = plt.subplots(
            nrows, ncols, figsize=(7 * ncols, 6 * nrows), squeeze=False
        )
        axes_flat = axes.flatten()

        any_plotted = False
        for idx, (metric_key, metric_label) in enumerate(ENCODING_METRICS):
            if _plot_metric_scatter(
                axes_flat[idx], common, metric_key, metric_label,
                strategy_a, strategy_b, domain_styles,
            ):
                any_plotted = True

        for idx in range(n_metrics, len(axes_flat)):
            axes_flat[idx].set_visible(False)

        if not any_plotted:
            plt.close(fig)
            continue

        handles, labels = axes_flat[0].get_legend_handles_labels()
        if handles:
            fig.legend(
                handles, labels, loc="center right", fontsize=7,
                bbox_to_anchor=(1.0, 0.5), ncol=1,
            )
        fig.suptitle(f"Encoding Size: {strategy_a} vs {strategy_b}", fontsize=13)
        fig.tight_layout(rect=[0, 0, 0.88, 0.95])
        pdf.savefig(fig)
        plt.close(fig)


def plot_domain_summaries(data: ExperimentData, pdf: PdfPages) -> None:
    """Per-domain grouped bar chart comparing strategies on key metrics."""
    colors = get_strategy_colors(data.strategies)

    for domain in data.domains:
        strat_data: Dict[str, List[SlurmJobResult]] = {}
        for strategy in data.strategies:
            jobs = [
                j
                for j in data.jobs
                if j.domain == domain
                and j.strategy == strategy
                and j.status == Status.SOLVED
                and j.stats
            ]
            if len(jobs) >= 3:
                strat_data[strategy] = jobs

        if len(strat_data) < 2:
            continue

        strategies_here = sorted(strat_data.keys())
        n_metrics = len(DOMAIN_SUMMARY_METRICS)

        fig, axes = plt.subplots(
            1, n_metrics, figsize=(3.5 * n_metrics, 5), squeeze=False
        )
        axes_flat = axes.flatten()

        width = 0.8 / len(strategies_here)

        for m_idx, (metric_key, metric_label) in enumerate(DOMAIN_SUMMARY_METRICS):
            ax = axes_flat[m_idx]
            for s_idx, strategy in enumerate(strategies_here):
                vals = [j.stats.get(metric_key, 0) for j in strat_data[strategy]]
                vals_sorted = sorted(vals)
                median_val = vals_sorted[len(vals) // 2]
                offset = (s_idx - len(strategies_here) / 2 + 0.5) * width
                ax.bar(
                    offset, median_val, width=width,
                    color=colors[strategy], alpha=0.8,
                    label=strategy if m_idx == 0 else None,
                )

            ax.set_title(metric_label, fontsize=9)
            ax.set_xticks([])
            ax.grid(True, axis="y", alpha=0.3)

        handles, labels = axes_flat[0].get_legend_handles_labels()
        if handles:
            fig.legend(
                handles, labels, loc="lower center", fontsize=7,
                ncol=min(4, len(strategies_here)),
                bbox_to_anchor=(0.5, 0.01),
            )

        n_instances = min(len(v) for v in strat_data.values())
        fig.suptitle(
            f"Domain: {domain} (median, n>={n_instances})", fontsize=13
        )
        fig.tight_layout(rect=[0, 0.08, 1, 0.93])
        pdf.savefig(fig)
        plt.close(fig)


# --- Stats Analysis: Report Orchestration ---

STATS_SECTIONS = {
    "z3-scatters": "Z3 metric scatter plots (pairwise strategy comparison)",
    "propagator": "Propagator impact analysis (auto-discovered pairs)",
    "time-breakdown": "Time breakdown stacked bars (per strategy)",
    "encoding": "Encoding size comparison (pairwise)",
    "domain-summary": "Per-domain summary (grouped bars)",
}

_STATS_CSV_DISPATCH = {
    "z3-scatters": ("stats_z3_comparison.csv", write_z3_comparison_csv),
    "propagator": ("stats_propagator_impact.csv", write_propagator_impact_csv),
    "domain-summary": ("stats_domain_aggregate.csv", write_domain_aggregate_csv),
}

_STATS_PLOT_DISPATCH = {
    "z3-scatters": plot_z3_metric_scatters,
    "propagator": plot_propagator_impact,
    "time-breakdown": plot_time_breakdowns,
    "encoding": plot_encoding_comparison,
    "domain-summary": plot_domain_summaries,
}


def generate_stats_report(
    data: ExperimentData,
    output_dir: Path,
    sections: Optional[List[str]] = None,
) -> None:
    """Generate stats-based analysis outputs."""
    has_stats = any(job.stats for job in data.jobs)
    if not has_stats:
        print("No stats data loaded, skipping stats analysis.", file=sys.stderr)
        return

    if sections is None:
        active = set(STATS_SECTIONS.keys())
    else:
        active = set(sections) & set(STATS_SECTIONS.keys())

    if not active:
        return

    print(
        f"\nGenerating stats analysis ({len(active)} sections)...",
        file=sys.stderr,
    )

    # Always write full dump when any stats section is active
    write_stats_full_csv(data, output_dir / "stats_full.csv")

    # Section-specific CSVs
    for name in STATS_SECTIONS:
        if name in active and name in _STATS_CSV_DISPATCH:
            csv_name, csv_fn = _STATS_CSV_DISPATCH[name]
            csv_fn(data, output_dir / csv_name)

    # All plots in one PDF
    pdf_path = output_dir / "stats_analysis.pdf"
    with PdfPages(str(pdf_path)) as pdf:
        for name in STATS_SECTIONS:
            if name in active and name in _STATS_PLOT_DISPATCH:
                print(
                    f"  Plotting: {STATS_SECTIONS[name]}...", file=sys.stderr
                )
                _STATS_PLOT_DISPATCH[name](data, pdf)

    print(f"Wrote {pdf_path}", file=sys.stderr)


# --- Main ---

def generate_all_outputs(
    data: ExperimentData,
    output_dir: Path,
    stats_sections: Optional[List[str]] = None,
) -> None:
    """Generate all output files.

    stats_sections: list of section names to generate, None = all, [] = skip stats.
    """
    output_dir.mkdir(parents=True, exist_ok=True)

    # CSVs
    write_instances_csv(data, output_dir / "instances.csv")
    write_summary_by_domain_csv(data, output_dir / "summary_by_domain.csv")
    write_summary_overall_csv(data, output_dir / "summary_overall.csv")

    # Plots
    plot_cactus(data, output_dir / "cactus.pdf")
    plot_all_scatters(data, output_dir)

    # Stats analysis
    if stats_sections is None or stats_sections:
        generate_stats_report(data, output_dir, stats_sections)

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
    section_names = ", ".join(STATS_SECTIONS.keys())
    parser.add_argument(
        "--stats-sections",
        default=None,
        help=f"Comma-separated list of stats sections to generate. "
        f"Available: {section_names}. Default: all",
    )
    parser.add_argument(
        "--skip-stats-sections",
        default=None,
        help="Comma-separated list of stats sections to skip",
    )
    parser.add_argument(
        "--no-stats",
        action="store_true",
        help="Skip all stats-based analysis",
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

    # Determine stats sections to run
    if args.no_stats:
        stats_sections: Optional[List[str]] = []
    elif args.stats_sections:
        stats_sections = [s.strip() for s in args.stats_sections.split(",")]
    elif args.skip_stats_sections:
        skip = {s.strip() for s in args.skip_stats_sections.split(",")}
        stats_sections = [s for s in STATS_SECTIONS if s not in skip]
    else:
        stats_sections = None  # all

    generate_all_outputs(data, Path(args.output_dir), stats_sections)


if __name__ == "__main__":
    main()
