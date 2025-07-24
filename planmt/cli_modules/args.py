#!/usr/bin/env python3
"""
Argument parsing and validation for planMT CLI.
"""

import argparse
import os


def parse_arguments():
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(
        description="planMT - A planning-as-satisfiability planner using Unified Planning",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --domain examples/domain.pddl --problem examples/problem.pddl
  %(prog)s -d domain.pddl -p problem.pddl --timeout 30 --verbose
  %(prog)s --domain domain.pddl --problem problem.pddl --parallelism forall --propagator forall
  %(prog)s --domain domain.pddl --problem problem.pddl --executable /path/to/planmt

For more information, visit: https://github.com/pyPMT/planMT
        """
    )
    
    parser.add_argument(
        "-d", "--domain",
        type=str,
        required=True,
        help="Path to the PDDL domain file"
    )
    
    parser.add_argument(
        "-p", "--problem", 
        type=str,
        required=True,
        help="Path to the PDDL problem file"
    )
    
    parser.add_argument(
        "--timeout",
        type=int,
        default=3600,
        help="Timeout for planning in seconds (default: 3600). Note is only checked between calls."
    )
    
    parser.add_argument(
        "--executable",
        type=str,
        help="Path to the planMT C++ executable (optional, auto-detected by default)"
    )
    
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Enable verbose output"
    )
    
    parser.add_argument(
        "--parallelism",
        type=str,
        choices=["sequential", "forall", "exists"],
        default="sequential",
        help="Parallelism strategy: sequential (exactly one action per timestep), "
             "forall (actions can execute in parallel if they don't conflict), "
             "or exists (at least one action must execute per timestep). Default: sequential"
    )
    
    parser.add_argument(
        "--propagator",
        type=str,
        choices=["null", "forall"],
        default="null",
        help="Propagator strategy: null (no propagation, default), "
             "or forall (forall-specific propagation). Default: null"
    )
    
    parser.add_argument(
        "--output-plan",
        type=str,
        help="Save the plan to a file (if found)"
    )
    
    parser.add_argument(
        "--version",
        action="version",
        version="planMT 0.0.1"
    )
    
    return parser.parse_args()


def validate_files(domain_file, problem_file):
    """Validate that the input files exist and are readable."""
    errors = []
    
    if not os.path.exists(domain_file):
        errors.append(f"Domain file not found: {domain_file}")
    elif not os.path.isfile(domain_file):
        errors.append(f"Domain path is not a file: {domain_file}")
    elif not os.access(domain_file, os.R_OK):
        errors.append(f"Domain file is not readable: {domain_file}")
    
    if not os.path.exists(problem_file):
        errors.append(f"Problem file not found: {problem_file}")
    elif not os.path.isfile(problem_file):
        errors.append(f"Problem path is not a file: {problem_file}")
    elif not os.access(problem_file, os.R_OK):
        errors.append(f"Problem file is not readable: {problem_file}")
    
    return errors


def validate_args(args):
    """Validate parsed arguments for consistency."""
    # Add any cross-argument validation here if needed
    _ = args  # Mark as used to avoid linting warning
    return []