#!/usr/bin/env python3
"""
Comprehensive benchmarking script for the RantanPlan planning system.

This script runs systematic performance comparisons across multiple PDDL domains
and instances using different parallelism semantics, propagators, and interference
analysis strategies (eager vs lazy). It uses GNU parallel for efficient CPU 
utilization and generates results tables showing the number of instances solved 
per approach within a given timeout.

Benchmark configurations include:
- Sequential vs parallel semantics (forall/exists)
- Various propagator strategies (null, forall, lazy_forall, exists)
- Eager vs lazy interference analysis (pre-computed vs on-demand)
"""
import os
import sys
import argparse
import json
import tempfile
import subprocess
import csv
from pathlib import Path
from collections import defaultdict
from rantanplan.planner_wrapper import RantanPlanPlanner

# --- Benchmark Configuration ---

# Get available strategies from C++ planner
def get_available_strategies():
    """Query the C++ planner for available strategies."""
    planner = RantanPlanPlanner()
    result = subprocess.run(
        [planner.executable_path, "/dev/null", "/dev/null", "--list-strategies"],
        capture_output=True, text=True, check=True
    )
    # Parse strategy names from output (format: "  strategy_name")
    strategies = []
    for line in result.stdout.strip().split('\n'):
        line = line.strip()
        # Skip header line and empty lines
        if line and not line.startswith('Available'):
            strategies.append(line)
    return strategies

# Benchmark strategies (excluding sequential for performance focus)
BENCHMARK_STRATEGIES = [
    "r2e",
    "lazy-r2e",
    #"dec",
    "exists-lazy-semantic-chain",
    #"forall-lazy-semantic-chain",
    #"exists-lazy",
    #"forall-lazy"
]

# Default timeout per solver run (seconds)
DEFAULT_TIMEOUT = 60

# Default number of parallel jobs (0 = auto-detect CPU count)
DEFAULT_JOBS = 0

# --- ANSI Color Codes for Output ---
class Colors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def print_pass(message):
    print(f"{Colors.OKGREEN}[PASS]{Colors.ENDC} {message}")

def print_fail(message):
    print(f"{Colors.FAIL}[FAIL]{Colors.ENDC} {message}")

def print_info(message):
    print(f"{Colors.OKBLUE}[INFO]{Colors.ENDC} {message}")

def print_header(message):
    print(f"\n{Colors.HEADER}{Colors.BOLD}{message}{Colors.ENDC}")

def print_warning(message):
    print(f"{Colors.WARNING}[WARN]{Colors.ENDC} {message}")


def discover_pddl_instances(root_dir):
    """
    Discovers all PDDL domain/problem pairs in the specified directory.
    Supports multiple directory structures:
    1. domain.pddl + instances/*.pddl (pddl-benchmark style)
    2. domain.pddl + problem.pddl (single instance per domain)
    3. Multiple domain/problem pairs in same directory
    
    Args:
        root_dir: The directory to search for PDDL files.
        
    Returns:
        A dictionary mapping domain names to lists of (domain_file, problem_file) tuples.
    """
    instances_by_domain = defaultdict(list)
    root_path = Path(root_dir)
    
    # Check if this looks like pddl-benchmark structure
    for domain_dir in root_path.iterdir():
        if not domain_dir.is_dir():
            continue
            
        domain_file = domain_dir / "domain.pddl"
        instances_dir = domain_dir / "instances"
        
        # Case 1: domain.pddl + instances/ directory (pddl-benchmark style)
        if domain_file.exists() and instances_dir.exists() and instances_dir.is_dir():
            domain_name = domain_dir.name
            
            for instance_file in instances_dir.glob("*.pddl"):
                instances_by_domain[domain_name].append((domain_file, instance_file))
            
            continue
    
    # If we didn't find any pddl-benchmark style directories, fall back to old logic
    if not instances_by_domain:
        for dirpath, _, filenames in os.walk(root_dir):
            domain_files = []
            problem_files = []
            
            # Find all domain and problem files in the current directory
            for filename in filenames:
                if filename.endswith(".pddl"):
                    if "domain" in filename.lower():
                        domain_files.append(Path(dirpath) / filename)
                    elif "problem" in filename.lower():
                        problem_files.append(Path(dirpath) / filename)
            
            # Handle different directory structures
            if len(domain_files) == 1 and len(problem_files) >= 1:
                # Single domain, one or more problems
                domain_file = domain_files[0]
                domain_name = Path(dirpath).name
                
                for problem_file in problem_files:
                    instances_by_domain[domain_name].append((domain_file, problem_file))
                    
            elif len(domain_files) > 1 and len(problem_files) > 1:
                # Multiple domains and problems - try to match by naming
                domain_name = Path(dirpath).name
                
                # Sort files to ensure consistent pairing
                domain_files.sort()
                problem_files.sort()
                
                # Simple pairing strategy: match by index or naming pattern
                for i, problem_file in enumerate(problem_files):
                    if i < len(domain_files):
                        domain_file = domain_files[i]
                    else:
                        domain_file = domain_files[0]  # Use first domain for extra problems
                        
                    instances_by_domain[domain_name].append((domain_file, problem_file))
    
    if not instances_by_domain:
        print_warning(f"No PDDL instances found in {root_dir}")
    
    return dict(instances_by_domain)


def generate_job_commands(instances_by_domain, strategy_names, timeout, verbose=False,
                         extra_flags=None):
    """
    Generates individual solver commands for GNU parallel execution.

    Args:
        extra_flags: Additional CLI flags to append to every planner invocation.

    Returns:
        List of (command, job_id, domain, config, instance_name) tuples
    """
    commands = []
    job_id = 0
    if extra_flags is None:
        extra_flags = []
    
    for domain_name, instances in instances_by_domain.items():
        for domain_file, problem_file in instances:
            for strategy_name in strategy_names:
                # Create unique job identifier
                instance_name = problem_file.stem
                config_name = strategy_name
                # Use simpler job_id format for easier parsing
                current_job_id = f"{domain_name}_{instance_name}_{config_name}_{job_id}"
                
                # Build rantanplan command
                cmd = [
                    "rantanplan",
                    "-d", str(domain_file),
                    "-p", str(problem_file),
                    "--strategy", strategy_name,
                    "--timeout", str(timeout)
                ]
                
                if verbose:
                    cmd.append("-v")

                # Append any extra flags (grounding, RPG, passthrough)
                cmd.extend(extra_flags)
                
                commands.append((cmd, current_job_id, domain_name, config_name, instance_name))
                job_id += 1
    
    return commands


def run_parallel_benchmark(commands, jobs, results_dir, timeout=60):
    """
    Executes benchmark commands using GNU parallel and collects results.
    
    Returns:
        Dictionary mapping job_ids to results
    """
    # Create results directory
    results_path = Path(results_dir)
    results_path.mkdir(exist_ok=True)
    
    # Write job commands to temporary file for GNU parallel
    with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.jobs') as f:
        for cmd, job_id, _, _, _ in commands:
            # Redirect output to individual result files
            output_file = results_path / f"{job_id}.out"
            error_file = results_path / f"{job_id}.err"
            
            cmd_str = ' '.join([f"'{arg}'" if ' ' in arg else arg for arg in cmd])
            # Add memory limit of 8GB (8388608 KB) using ulimit -v
            f.write(f"ulimit -v 8388608; {cmd_str} > '{output_file}' 2> '{error_file}'\n")

        jobs_file = f.name
    
    try:
        # Run GNU parallel with proper input redirection and timeout
        parallel_cmd = ["parallel"]
        if jobs > 0:
            parallel_cmd.extend(["--jobs", str(jobs)])
        parallel_cmd.extend(["--joblog", str(results_path / "parallel.log")])
        # Add timeout as safety net (slightly longer than rantanplan timeout)
        parallel_cmd.extend(["--timeout", str(timeout + 10)])  # +10s buffer
        parallel_cmd.extend(["-a", jobs_file])  # Use -a to read from file
        
        print_info(f"Running {len(commands)} benchmark jobs with GNU parallel...")
        result = subprocess.run(parallel_cmd, capture_output=True, text=True)
        
        if result.returncode != 0:
            print_warning(f"GNU parallel returned code {result.returncode} (some jobs may have timed out)")
            if result.stderr:
                print_warning(f"Stderr: {result.stderr}")
            # Continue processing results even if parallel had issues
        
        # Parse results
        return parse_results(commands, results_path)
        
    finally:
        # Clean up temporary jobs file
        os.unlink(jobs_file)


def parse_parallel_log(log_file):
    """
    Parses GNU parallel log file to extract timing information.
    
    Returns:
        Dictionary mapping job_id to timing info
    """
    timing_info = {}
    
    if not log_file.exists():
        print_warning(f"Parallel log file not found: {log_file}")
        return timing_info
    
    try:
        with open(log_file, 'r') as f:
            # Skip header line
            next(f, None)
            
            for line in f:
                parts = line.strip().split('\t')
                if len(parts) >= 8:
                    start_time = parts[2]
                    runtime = parts[3]
                    exit_code = parts[6] if len(parts) > 6 else "0"
                    command = parts[8] if len(parts) > 8 else ""
                    
                    # Extract job_id from command (it's in the output filename)
                    job_id = None
                    if "> '" in command and ".out'" in command:
                        # Find the output file path more robustly
                        out_start = command.find("> '") + 3
                        out_end = command.find(".out'", out_start)
                        if out_start > 2 and out_end > out_start:
                            full_path = command[out_start:out_end]
                            # Extract just the filename without directory
                            job_id = full_path.split('/')[-1] if '/' in full_path else full_path
                    
                    if job_id and runtime:
                        try:
                            runtime_float = float(runtime.strip())
                            exit_code_int = int(exit_code) if exit_code.strip().isdigit() else 1
                            
                            timing_info[job_id] = {
                                'runtime': runtime_float,
                                'exit_code': exit_code_int,
                                'start_time': start_time
                            }
                        except ValueError:
                            # Skip malformed entries
                            continue
                            
    except Exception as e:
        print_warning(f"Failed to parse parallel.log: {e}")
    
    return timing_info


def parse_results(commands, results_path):
    """
    Parses individual job results and aggregates by domain and configuration.
    
    Returns:
        Tuple of (success_counts, detailed_results)
    """
    success_counts = defaultdict(lambda: defaultdict(int))
    detailed_results = []
    
    # Parse timing information from parallel log
    timing_info = parse_parallel_log(results_path / "parallel.log")
    print_info(f"Parsed timing info for {len(timing_info)} jobs")
    
    for _, job_id, domain, config, instance_name in commands:
        output_file = results_path / f"{job_id}.out"
        
        # Check if solver succeeded
        success = False
        if output_file.exists():
            try:
                with open(output_file, 'r') as f:
                    content = f.read()
                    # Look for success indicators in output
                    if ("Plan found" in content or "SOLVED" in content or "Solution found" in content or
                        "Status: PlanGenerationResultStatus.SOLVED_SATISFICING" in content or
                        "Status: PlanGenerationResultStatus.SOLVED_OPTIMALLY" in content):
                        success = True
                    # Also check that it's not a timeout/failure
                    elif ("Status: PlanGenerationResultStatus.TIMEOUT" in content or
                          "Status: PlanGenerationResultStatus.UNSOLVABLE_PROVEN" in content or
                          "Status: PlanGenerationResultStatus.UNSOLVABLE_INCOMPLETELY" in content or
                          "No plan found" in content or
                          "Planner timed out" in content):
                        success = False
            except:
                pass
        
        # Get timing information
        runtime = 0.0
        exit_code = 1
        if job_id in timing_info:
            runtime = timing_info[job_id]['runtime']
            exit_code = timing_info[job_id]['exit_code']
            # If exit code is 0 but we didn't find success indicators, check exit code
            if not success and exit_code == 0:
                success = True
        else:
            # Debug: print first few mismatched job_ids to help debug
            if len(detailed_results) < 5:  # Only print first few for debugging
                available_keys = list(timing_info.keys())[:3]
                print_warning(f"No timing info for job_id '{job_id}'. Available keys sample: {available_keys}")
        
        # Record detailed result
        detailed_results.append({
            'domain': domain,
            'instance': instance_name,
            'config': config,
            'success': success,
            'runtime': runtime,
            'exit_code': exit_code
        })
        
        # Count successes
        if success:
            success_counts[domain][config] += 1
    
    return dict(success_counts), detailed_results


def generate_summary_csv(results, strategy_names, output_file):
    """
    Generates CSV with number of instances solved per domain and configuration.
    """
    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        
        # Write header
        writer.writerow(['Domain'] + strategy_names)
        
        # Write data rows
        for domain in sorted(results.keys()):
            row = [domain]
            for strategy in strategy_names:
                count = results[domain].get(strategy, 0)
                row.append(count)
            writer.writerow(row)
    
    print_info(f"Summary results written to {output_file}")


def generate_detailed_csv(detailed_results, output_file):
    """
    Generates CSV with detailed timing information for each domain/instance/config combination.
    """
    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        
        # Write header
        writer.writerow(['Domain', 'Instance', 'Configuration', 'Success', 'Runtime_Seconds', 'Exit_Code'])
        
        # Sort results for consistent output
        sorted_results = sorted(detailed_results, key=lambda x: (x['domain'], x['instance'], x['config']))
        
        # Write data rows
        for result in sorted_results:
            writer.writerow([
                result['domain'],
                result['instance'], 
                result['config'],
                'YES' if result['success'] else 'NO',
                f"{result['runtime']:.3f}",
                result['exit_code']
            ])
    
    print_info(f"Detailed results written to {output_file}")


def print_summary_table(results, strategy_names):
    """
    Prints a simple text summary table to console.
    """
    print("\n# Benchmark Summary")
    print("\nInstances solved per domain:")
    print()
    
    # Calculate column widths
    max_domain_len = max(len(domain) for domain in results.keys()) if results else 6
    domain_width = max(max_domain_len, 6)
    
    # Print header
    header = f"{'Domain':<{domain_width}}"
    for strategy in strategy_names:
        header += f" {strategy:>20}"
    print(header)
    print("-" * len(header))
    
    # Print data rows
    for domain in sorted(results.keys()):
        row = f"{domain:<{domain_width}}"
        for strategy in strategy_names:
            count = results[domain].get(strategy, 0)
            row += f" {count:>20}"
        print(row)


def main():
    """Main entry point for the benchmark script."""
    parser = argparse.ArgumentParser(description="Benchmark script for the RantanPlan planner.")
    parser.add_argument(
        "pddl_dir",
        help="Directory containing PDDL domains and problems to benchmark"
    )
    parser.add_argument(
        "--timeout", "-t",
        type=int,
        default=DEFAULT_TIMEOUT,
        help=f"Timeout per solver run in seconds (default: {DEFAULT_TIMEOUT})"
    )
    parser.add_argument(
        "--jobs", "-j",
        type=int,
        default=DEFAULT_JOBS,
        help=f"Number of parallel jobs (default: auto-detect CPU count)"
    )
    parser.add_argument(
        "--output", "-o",
        help="Output file for results table (default: print to stdout)"
    )
    parser.add_argument(
        "--results-dir",
        default="benchmark_results",
        help="Directory to store individual job results (default: benchmark_results)"
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Enable verbose output from the planner"
    )
    parser.add_argument(
        "--configs",
        help="JSON file with custom configuration list (overrides default)"
    )

    # Grounding options
    parser.add_argument(
        "--up-grounding",
        action="store_true",
        help="Use Unified Planning cross-product grounding instead of C++ reachability-based grounding"
    )

    # Generic passthrough for any additional planner flags
    parser.add_argument(
        "--extra-args",
        nargs=argparse.REMAINDER,
        default=[],
        help="Extra arguments passed through to the planner verbatim "
             "(e.g. --extra-args --no-persist-clauses --symmetries)"
    )

    args = parser.parse_args()
    
    # Load strategies
    strategies = BENCHMARK_STRATEGIES
    if args.configs:
        with open(args.configs, 'r') as f:
            strategies = json.load(f)

    # Get available strategies from C++ planner and validate
    available_strategies = get_available_strategies()
    valid_strategies = []
    for strategy_name in strategies:
        if strategy_name in available_strategies:
            valid_strategies.append(strategy_name)
        else:
            print_warning(f"Skipping unknown strategy '{strategy_name}' (not in available strategies)")
    
    print_header("--- Starting RantanPlan Benchmark Suite ---")
    print_info(f"PDDL directory: {args.pddl_dir}")
    print_info(f"Timeout per run: {args.timeout}s")
    print_info(f"Parallel jobs: {args.jobs if args.jobs > 0 else 'auto'}")
    print_info(f"Strategies: {len(valid_strategies)}")
    
    # Discover PDDL instances
    instances_by_domain = discover_pddl_instances(args.pddl_dir)
    if not instances_by_domain:
        sys.exit(1)
    
    total_instances = sum(len(instances) for instances in instances_by_domain.values())
    total_jobs = total_instances * len(valid_strategies)
    
    print_info(f"Found {len(instances_by_domain)} domains with {total_instances} total instances")
    print_info(f"Total benchmark jobs: {total_jobs}")
    
    # Build extra flags from grounding/RPG options + passthrough
    extra_flags = []
    if args.up_grounding:
        extra_flags.append("--up-grounding")
    extra_flags.extend(args.extra_args)

    if extra_flags:
        print_info(f"Extra planner flags: {' '.join(extra_flags)}")

    # Generate job commands
    commands = generate_job_commands(instances_by_domain, valid_strategies, args.timeout, args.verbose,
                                    extra_flags=extra_flags)
    
    # Run parallel benchmark
    success_counts, detailed_results = run_parallel_benchmark(commands, args.jobs, args.results_dir, args.timeout)
    
    # Generate output files
    base_name = Path(args.output).stem if args.output else "benchmark_results"
    base_dir = Path(args.output).parent if args.output else Path(".")
    
    summary_file = base_dir / f"{base_name}_summary.csv"
    detailed_file = base_dir / f"{base_name}_detailed.csv"
    
    generate_summary_csv(success_counts, valid_strategies, summary_file)
    generate_detailed_csv(detailed_results, detailed_file)
    
    # Print summary to console
    print_summary_table(success_counts, valid_strategies)
    
    print_header("--- Benchmark Complete ---")


if __name__ == "__main__":
    # Ensure the rantanplan module is in the Python path
    sys.path.insert(0, os.path.abspath(os.path.dirname(__file__)))
    main()
