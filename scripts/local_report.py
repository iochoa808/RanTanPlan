#!/usr/bin/env python3
"""
Report generator for RantanPlan benchmark results.

Generates a PDF report with:
1. A cactus (survival) plot comparing configurations by solving time
2. Per-instance timestep plots showing per-step solving time

Supports two modes:
- Single directory: compare configurations within one benchmark run
- Two directories: compare matching configurations across two runs
"""
import argparse
import re
import sys
from dataclasses import dataclass, field
from itertools import combinations
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages


# --- Data structures ---

@dataclass
class TimestepData:
    timestep: int
    formula_time: float
    solve_time: float
    step_time: float
    memory_mb: int


@dataclass
class JobResult:
    domain: str
    instance: str
    config: str
    out_file: Path
    wall_time: float
    exit_code: int
    timeout_value: float
    solved: bool = False
    total_time: float | None = None
    horizon: int | None = None
    num_actions: int | None = None
    timesteps: list[TimestepData] = field(default_factory=list)


@dataclass
class BenchmarkRun:
    directory: Path
    label: str
    jobs: list[JobResult]
    timeout: float

    @property
    def configs(self) -> set[str]:
        return {j.config for j in self.jobs}


# --- Regex patterns ---

RE_TIMESTEP = re.compile(
    r"\[Solving T(\d+)\s*\]\s+"
    r"formula:\s+([\d.]+)s\s+\|\s+"
    r"solve:\s+([\d.]+)s\s+\|\s+"
    r"step:\s+([\d.]+)s\s+\|\s+"
    r"mem:\s+(\d+)MB"
)
RE_PLAN_FOUND = re.compile(
    r"\*\*\* PLAN FOUND: horizon=(\d+), actions=(\d+) \(total time: ([\d.]+)s\)"
)
RE_OPTIMAL_FOUND = re.compile(
    r"\*\*\* OPTIMAL PLAN FOUND: horizon=(\d+), actions=(\d+), cost=[\d.]+.*\(total time: ([\d.]+)s\)"
)
RE_BEST_FOUND = re.compile(
    r"\*\*\* BEST PLAN FOUND: horizon=(\d+), actions=(\d+), cost=[\d.]+.*\(total time: ([\d.]+)s\)"
)
RE_TIMEOUT = re.compile(r"\*\*\* TIMEOUT")
RE_NO_PLAN = re.compile(r"\*\*\* NO PLAN FOUND")
RE_STATUS = re.compile(r"Status: PlanGenerationResultStatus\.(\w+)")
RE_OUT_REDIRECT = re.compile(r"> '([^']+\.out)'")


# --- Parsing ---

def parse_command_flags(command: str) -> dict:
    """Extract -d, -p, --strategy, --timeout, and output file from a parallel command."""
    result = {}

    # Extract output file from redirect
    m = RE_OUT_REDIRECT.search(command)
    if m:
        result["out_file"] = m.group(1)

    # Tokenize the command (simple split, handles most cases)
    # Remove the ulimit prefix and redirects for cleaner parsing
    cmd_part = command.split(">")[0].strip()
    tokens = cmd_part.split()

    i = 0
    while i < len(tokens):
        tok = tokens[i]
        if tok in ("-d", "-p", "--strategy", "--timeout") and i + 1 < len(tokens):
            flag_name = tok.lstrip("-")
            if flag_name == "d":
                flag_name = "domain_path"
            elif flag_name == "p":
                flag_name = "problem_path"
            result[flag_name] = tokens[i + 1]
            i += 2
        else:
            i += 1

    return result


def parse_parallel_log(log_path: Path) -> list[dict]:
    """Parse GNU parallel log file, extracting job metadata from each row."""
    jobs = []
    with open(log_path, "r") as f:
        header = next(f, None)
        if header is None:
            return jobs

        for line in f:
            parts = line.strip().split("\t")
            if len(parts) < 9:
                continue

            wall_time = float(parts[3].strip())
            exit_code = int(parts[6].strip())
            command = parts[8]

            flags = parse_command_flags(command)
            if not flags.get("domain_path") or not flags.get("problem_path"):
                continue

            # Derive domain name from path
            domain_path = Path(flags["domain_path"])
            domain_name = domain_path.parent.name
            # If the domain file is directly in a named directory like pddl/bench/depots/domain.pddl
            # then parent.name = "depots". If it's in instances/ subfolder, go one level up.
            if domain_name == "instances":
                domain_name = domain_path.parent.parent.name

            # Instance name from problem path
            instance_name = Path(flags["problem_path"]).stem

            jobs.append({
                "domain": domain_name,
                "instance": instance_name,
                "config": flags.get("strategy", "unknown"),
                "timeout": float(flags.get("timeout", 3600)),
                "wall_time": wall_time,
                "exit_code": exit_code,
                "out_file": flags.get("out_file", ""),
            })

    return jobs


def parse_out_file(out_path: Path) -> dict:
    """Parse a .out file for per-timestep data, solution status, and timing."""
    result = {
        "solved": False,
        "total_time": None,
        "horizon": None,
        "num_actions": None,
        "timesteps": [],
    }

    if not out_path.exists():
        return result

    try:
        content = out_path.read_text(errors="replace")
    except OSError:
        return result

    # Parse timestep lines
    for m in RE_TIMESTEP.finditer(content):
        result["timesteps"].append(TimestepData(
            timestep=int(m.group(1)),
            formula_time=float(m.group(2)),
            solve_time=float(m.group(3)),
            step_time=float(m.group(4)),
            memory_mb=int(m.group(5)),
        ))

    # Check for plan found (try all variants)
    for pattern in (RE_PLAN_FOUND, RE_OPTIMAL_FOUND, RE_BEST_FOUND):
        m = pattern.search(content)
        if m:
            result["solved"] = True
            result["horizon"] = int(m.group(1))
            result["num_actions"] = int(m.group(2))
            result["total_time"] = float(m.group(3))
            break

    # Also check status line as fallback
    if not result["solved"]:
        m = RE_STATUS.search(content)
        if m and m.group(1) in ("SOLVED_SATISFICING", "SOLVED_OPTIMALLY"):
            result["solved"] = True

    return result


def resolve_out_file(results_dir: Path, logged_path: str) -> Path:
    """Resolve the .out file path from the parallel.log entry.

    Prioritizes files within results_dir, since the logged path may point
    to a different directory (e.g., an older run's output folder).
    """
    # First priority: look in the results directory by filename
    p = results_dir / Path(logged_path).name
    if p.exists():
        return p

    # Try the logged path as-is (absolute or relative to cwd)
    p = Path(logged_path)
    if p.exists():
        return p

    # Try joining results_dir with the full logged path
    p = results_dir / logged_path
    if p.exists():
        return p

    # Return best guess
    return results_dir / Path(logged_path).name


def load_benchmark_run(directory: Path) -> BenchmarkRun:
    """Load all benchmark results from a directory."""
    log_path = directory / "parallel.log"
    if not log_path.exists():
        print(f"Error: {log_path} not found", file=sys.stderr)
        sys.exit(1)

    raw_jobs = parse_parallel_log(log_path)
    if not raw_jobs:
        print(f"Error: No jobs found in {log_path}", file=sys.stderr)
        sys.exit(1)

    jobs = []
    max_timeout = 0.0

    for rj in raw_jobs:
        out_path = resolve_out_file(directory, rj["out_file"])
        parsed = parse_out_file(out_path)

        timeout_val = rj["timeout"]
        max_timeout = max(max_timeout, timeout_val)

        # Use planner-reported total_time if available, else wall time
        if parsed["solved"] and parsed["total_time"] is not None:
            total_time = parsed["total_time"]
        elif parsed["solved"]:
            total_time = rj["wall_time"]
        else:
            total_time = None

        job = JobResult(
            domain=rj["domain"],
            instance=rj["instance"],
            config=rj["config"],
            out_file=out_path,
            wall_time=rj["wall_time"],
            exit_code=rj["exit_code"],
            timeout_value=timeout_val,
            solved=parsed["solved"],
            total_time=total_time,
            horizon=parsed["horizon"],
            num_actions=parsed["num_actions"],
            timesteps=parsed["timesteps"],
        )
        jobs.append(job)

    label = directory.name
    return BenchmarkRun(
        directory=directory,
        label=label,
        jobs=jobs,
        timeout=max_timeout,
    )


def validate_matching_configs(run_a: BenchmarkRun, run_b: BenchmarkRun) -> None:
    """Validate that two runs have matching configurations."""
    configs_a = run_a.configs
    configs_b = run_b.configs

    only_a = configs_a - configs_b
    only_b = configs_b - configs_a
    common = configs_a & configs_b

    if not common:
        print("Error: No common configurations between the two runs.", file=sys.stderr)
        print(f"  {run_a.label}: {sorted(configs_a)}", file=sys.stderr)
        print(f"  {run_b.label}: {sorted(configs_b)}", file=sys.stderr)
        sys.exit(1)

    if only_a:
        print(f"Warning: configs only in {run_a.label}: {sorted(only_a)}", file=sys.stderr)
    if only_b:
        print(f"Warning: configs only in {run_b.label}: {sorted(only_b)}", file=sys.stderr)


# --- Plot data builders ---

def instance_key(job: JobResult) -> str:
    return f"{job.domain}/{job.instance}"


def build_cactus_data(runs: list[BenchmarkRun], timeout: float) -> dict[str, list[float]]:
    """Build cactus plot data: {legend_label: sorted_times}."""
    two_dir = len(runs) > 1
    data = {}

    for run in runs:
        by_config: dict[str, list[float]] = {}
        for job in run.jobs:
            times = by_config.setdefault(job.config, [])
            if job.solved and job.total_time is not None:
                times.append(job.total_time)
            else:
                times.append(timeout)

        for config, times in by_config.items():
            times.sort()
            label = f"{run.label}/{config}" if two_dir else config
            data[label] = times

    return data


def build_timestep_data(
    runs: list[BenchmarkRun],
) -> dict[str, list[tuple[str, list[TimestepData]]]]:
    """Group per-timestep data by instance. Returns {instance_key: [(label, timesteps)]}."""
    two_dir = len(runs) > 1
    result: dict[str, list[tuple[str, list[TimestepData]]]] = {}

    for run in runs:
        for job in run.jobs:
            if not job.timesteps:
                continue
            key = instance_key(job)
            label = f"{run.label}/{job.config}" if two_dir else job.config
            result.setdefault(key, []).append((label, job.timesteps))

    return result


# --- Plotting ---

def get_color_map(labels: list[str]) -> dict[str, str]:
    """Assign consistent colors to labels."""
    cmap = matplotlib.colormaps["tab10"]
    colors = {}
    for i, label in enumerate(sorted(set(labels))):
        colors[label] = cmap(i % 10)
    return colors


def plot_cactus(ax, cactus_data: dict[str, list[float]], timeout: float, title: str) -> None:
    """Render cactus/survival plot."""
    colors = get_color_map(list(cactus_data.keys()))

    for label, times in sorted(cactus_data.items()):
        x = list(range(1, len(times) + 1))
        ax.plot(x, times, marker="o", markersize=4, label=label, color=colors[label])

    ax.axhline(y=timeout, color="gray", linestyle="--", linewidth=0.8, label=f"timeout ({timeout:.0f}s)")
    ax.set_xlabel("Number of instances")
    ax.set_ylabel("Time (s)")
    ax.set_yscale("log")
    ax.set_title(title)
    ax.legend(fontsize=8, loc="best")
    ax.grid(True, which="both", alpha=0.3)


MARKERS = ["o", "s", "^", "D", "v", "P", "X", "*", "p", "h", "<", ">", "d", "H", "8"]


def get_domain_styles(domains: list[str]) -> dict[str, tuple[str, object]]:
    """Assign (marker, color) pairs to domains."""
    cmap = matplotlib.colormaps["tab20"]
    styles: dict[str, tuple[str, object]] = {}
    for i, d in enumerate(sorted(domains)):
        marker = MARKERS[i % len(MARKERS)]
        color = cmap(i % 20)
        styles[d] = (marker, color)
    return styles


def build_scatter_data(
    runs: list["BenchmarkRun"], timeout: float,
) -> tuple[list[str], list[str], dict[tuple[str, str, str], float]]:
    """Build lookup for scatter plots.

    Returns (configs, domains, times_by_key) where
    times_by_key maps (domain/instance, config_label) -> time (timeout if unsolved).
    """
    two_dir = len(runs) > 1
    configs: list[str] = []
    domains: set[str] = set()
    times: dict[tuple[str, str], float] = {}

    for run in runs:
        for job in run.jobs:
            label = f"{run.label}/{job.config}" if two_dir else job.config
            key = (instance_key(job), label)
            if job.solved and job.total_time is not None:
                times[key] = job.total_time
            else:
                times[key] = timeout
            domains.add(job.domain)

    # Collect unique config labels preserving order
    seen: set[str] = set()
    for run in runs:
        for job in run.jobs:
            label = f"{run.label}/{job.config}" if two_dir else job.config
            if label not in seen:
                configs.append(label)
                seen.add(label)

    return configs, sorted(domains), times


def plot_scatter_pair(
    ax,
    config_a: str,
    config_b: str,
    instances: list[str],
    times: dict[tuple[str, str], float],
    domain_styles: dict[str, tuple[str, object]],
    timeout: float,
    log_scale: bool,
) -> None:
    """Plot a single scatter comparison on the given axes."""
    plotted_domains: set[str] = set()

    for inst in instances:
        key_a = (inst, config_a)
        key_b = (inst, config_b)
        if key_a not in times or key_b not in times:
            continue

        domain = inst.split("/")[0]
        marker, color = domain_styles.get(domain, ("o", "gray"))
        label = domain if domain not in plotted_domains else None
        plotted_domains.add(domain)

        ax.scatter(
            times[key_a], times[key_b],
            marker=marker, color=color, s=30, alpha=0.7,
            label=label, edgecolors="none",
        )

    # Diagonal reference line
    lo = 0.01 if log_scale else 0
    hi = timeout * 1.1
    ax.plot([lo, hi], [lo, hi], "k--", linewidth=0.5, alpha=0.5)

    # Timeout reference lines
    ax.axhline(y=timeout, color="gray", linestyle=":", linewidth=0.5, alpha=0.5)
    ax.axvline(x=timeout, color="gray", linestyle=":", linewidth=0.5, alpha=0.5)

    ax.set_xlabel(config_a)
    ax.set_ylabel(config_b)

    if log_scale:
        ax.set_xscale("log")
        ax.set_yscale("log")
        ax.set_title(f"{config_a} vs {config_b} (log)")
    else:
        ax.set_title(f"{config_a} vs {config_b} (linear)")

    ax.set_xlim(left=lo if log_scale else -timeout * 0.02, right=hi)
    ax.set_ylim(bottom=lo if log_scale else -timeout * 0.02, top=hi)
    ax.set_aspect("equal", adjustable="box")
    ax.grid(True, which="both", alpha=0.3)


def plot_timestep(ax, inst_key: str, series: list[tuple[str, list[TimestepData]]], timeout: float) -> None:
    """Render per-instance timestep plot."""
    labels = [s[0] for s in series]
    colors = get_color_map(labels)

    for label, timesteps in series:
        ts_sorted = sorted(timesteps, key=lambda t: t.timestep)
        x = [t.timestep for t in ts_sorted]
        y = [t.step_time for t in ts_sorted]
        ax.plot(x, y, marker="o", markersize=3, label=label, color=colors[label])

    ax.axhline(y=timeout, color="gray", linestyle="--", linewidth=0.8, label=f"timeout ({timeout:.0f}s)")
    ax.set_xlabel("Timestep")
    ax.set_ylabel("Step time (s)")
    ax.set_yscale("log")
    ax.set_title(inst_key)
    ax.legend(fontsize=8, loc="best")
    ax.grid(True, which="both", alpha=0.3)


# --- PDF generation ---

def generate_pdf(
    runs: list[BenchmarkRun],
    output_path: Path,
    title: str,
    timeout: float,
) -> None:
    """Generate the full PDF report."""
    num_pages = 0
    with PdfPages(str(output_path)) as pdf:
        # Page 1: Cactus plot
        fig, ax = plt.subplots(figsize=(10, 6))
        cactus_data = build_cactus_data(runs, timeout)
        plot_cactus(ax, cactus_data, timeout, title)
        fig.tight_layout()
        pdf.savefig(fig)
        plt.close(fig)
        num_pages += 1

        # Pairwise scatter plots
        configs, domains, times = build_scatter_data(runs, timeout)
        if len(configs) >= 2:
            domain_styles = get_domain_styles(domains)
            # Collect all instance keys
            instances = sorted({
                instance_key(job)
                for run in runs
                for job in run.jobs
            })

            for config_a, config_b in combinations(configs, 2):
                fig, (ax_lin, ax_log) = plt.subplots(1, 2, figsize=(16, 7))
                plot_scatter_pair(ax_lin, config_a, config_b, instances, times, domain_styles, timeout, False)
                plot_scatter_pair(ax_log, config_a, config_b, instances, times, domain_styles, timeout, True)

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

                fig.suptitle(f"{config_a} vs {config_b}", fontsize=13)
                fig.tight_layout(rect=[0, 0, 0.85, 0.95])
                pdf.savefig(fig)
                plt.close(fig)
                num_pages += 1

        # Per-instance timestep pages
        timestep_data = build_timestep_data(runs)
        for inst_key in sorted(timestep_data.keys()):
            series = timestep_data[inst_key]
            fig, ax = plt.subplots(figsize=(10, 6))
            plot_timestep(ax, inst_key, series, timeout)
            fig.tight_layout()
            pdf.savefig(fig)
            plt.close(fig)
            num_pages += 1

    print(f"Report written to {output_path} ({num_pages} pages)")


# --- CLI ---

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate PDF report from RantanPlan benchmark results."
    )
    parser.add_argument(
        "dirs",
        nargs="+",
        help="One or two benchmark results directories",
    )
    parser.add_argument(
        "-o", "--output",
        default="report.pdf",
        help="Output PDF path (default: report.pdf)",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=None,
        help="Override timeout value for cactus plot (auto-detected if not set)",
    )
    parser.add_argument(
        "--title",
        default=None,
        help="Report title (default: auto-generated)",
    )

    args = parser.parse_args()

    if len(args.dirs) > 2:
        parser.error("At most two directories can be compared")

    return args


def main():
    args = parse_args()

    dirs = [Path(d) for d in args.dirs]
    for d in dirs:
        if not d.is_dir():
            print(f"Error: {d} is not a directory", file=sys.stderr)
            sys.exit(1)

    runs = [load_benchmark_run(d) for d in dirs]

    if len(runs) == 2:
        validate_matching_configs(runs[0], runs[1])

    # Determine timeout
    timeout = args.timeout
    if timeout is None:
        timeout = max(r.timeout for r in runs)

    # Determine title
    if args.title:
        title = args.title
    elif len(runs) == 1:
        title = f"Benchmark Report: {runs[0].label}"
    else:
        title = f"Benchmark Comparison: {runs[0].label} vs {runs[1].label}"

    generate_pdf(runs, Path(args.output), title, timeout)


if __name__ == "__main__":
    main()
