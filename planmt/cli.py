#!/usr/bin/env python3
"""
planMT CLI - Command Line Interface for the planMT planning system.

This CLI provides an interface to the planMT planner, which uses a planning-as-satisfiability 
approach with a C++ backend and Unified Planning frontend.
"""

import argparse
import os
import sys
from importlib import metadata
from typing import Any, Dict
from unified_planning.engines.results import LogLevel


def parse_arguments():
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(
        description="planMT - A planning-as-satisfiability planner",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s -d domain.pddl -p problem.pddl --strategy seq
  %(prog)s -d domain.pddl -p problem.pddl --strategy forall-lazy --timeout 60
  %(prog)s --list-strategies

Use --list-strategies to see available strategies and their descriptions.
        """
    )

    # Required arguments
    parser.add_argument("-d", "--domain", type=str,
                       help="Path to the PDDL domain file")
    parser.add_argument("-p", "--problem", type=str,
                       help="Path to the PDDL problem file")

    # Strategy selection
    parser.add_argument("--strategy", type=str,
                       help="Planning strategy to use")

    # List strategies
    parser.add_argument("--list-strategies", action="store_true",
                       help="List available strategies and exit")
    
    # Optional global parameters
    parser.add_argument("--timeout", type=int, default=3600,
                       help="Timeout in seconds (default: 3600)")
    parser.add_argument("--max-steps", type=int,
                       help="Maximum planning steps")
    parser.add_argument("--executable", type=str,
                       help="Path to planMT C++ executable")
    parser.add_argument("--output-plan", type=str,
                       help="Save plan to file")
    parser.add_argument("--stats-file", type=str,
                       help="Save statistics to file")
    
    # Verbosity
    parser.add_argument("-v", "--verbose", action="count", default=0,
                       help="Increase verbosity (-v: verbose, -vv: debug)")
    parser.add_argument("--silent", action="store_true",
                       help="Suppress all output")
    
    # Additional flags that work with all strategies
    parser.add_argument("--no-persist-clauses", action="store_true",
                       help="Disable Z3 clause persistence")
    parser.add_argument("--detect-symmetries", action="store_true",
                       help="Enable symmetry detection and output")
    parser.add_argument("--no-cnf-normalization", action="store_true",
                       help="Disable CNF normalization of goals and preconditions")
    parser.add_argument("--no-action-removal", action="store_true",
                       help="Disable RPG-based action removal optimization")
    
    parser.add_argument(
        "--version",
        action="version",
        version=f"planMT {metadata.version('planmt')}"
    )
    
    return parser.parse_args()








def solve_problem(problem, args):
    """Solve problem using selected strategy."""
    try:
        from unified_planning.shortcuts import OneshotPlanner
        import planmt
    except ImportError as e:
        print(f"Error: Failed to import dependencies: {e}")
        return None

    # Build planner parameters with strategy name
    planner_params: Dict[str, object] = {
        'strategy': args.strategy,
    }

    if args.executable:
        planner_params['executable_path'] = args.executable
    if args.max_steps:
        planner_params['max_steps'] = args.max_steps
    if args.stats_file:
        planner_params['stats_file'] = args.stats_file
    if args.no_persist_clauses:
        planner_params['no_persist_clauses'] = True
    if args.detect_symmetries:
        planner_params['detect_symmetries'] = True
    if args.no_cnf_normalization:
        planner_params['no_cnf_normalization'] = True
    if args.no_action_removal:
        planner_params['no_action_removal'] = True

    # Handle verbosity
    if args.silent:
        planner_params['verbosity'] = "silent"
    elif args.verbose == 1:
        planner_params['verbosity'] = "verbose"
    elif args.verbose >= 2:
        planner_params['verbosity'] = "debug"

    # Show configuration info
    if not args.silent and args.verbose >= 1:
        print(f"Strategy: {args.strategy}")

    try:
        # Cast to Any to avoid static typing issues with UP engines
        PlannerClass: Any = OneshotPlanner
        with PlannerClass(name='planMT', params=planner_params) as planner:  # type: ignore[func-returns-value]
            result = planner.solve(problem, timeout=args.timeout)  # type: ignore[attr-defined]
            return result
    except Exception as e:
        print(f"Error during planning: {e}")
        return None






def main():
    """Main entry point."""
    args = parse_arguments()

    # Handle --list-strategies
    if args.list_strategies:
        # Call the C++ executable with --list-strategies
        import planmt
        executable_path = planmt.planner_wrapper.planMTPlanner(**{})._find_executable(None)
        try:
            import subprocess
            # C++ planner requires dummy protobuf files as positional args
            result = subprocess.run([executable_path, "/dev/null", "/dev/null", "--list-strategies"],
                                  capture_output=True, text=True)
            print(result.stdout)
            sys.exit(0)
        except Exception as e:
            print(f"Error listing strategies: {e}")
            sys.exit(1)

    # Validate required arguments for solving
    if not args.domain or not args.problem:
        print("Error: --domain and --problem are required (or use --list-strategies)")
        sys.exit(1)

    if not args.strategy:
        print("Error: --strategy is required")
        sys.exit(1)

    # Validate files
    for file_path in [args.domain, args.problem]:
        if not os.path.exists(file_path):
            print(f"Error: File not found: {file_path}")
            sys.exit(1)

    # Parse problem
    try:
        from unified_planning.io import PDDLReader
        reader = PDDLReader()
        problem = reader.parse_problem(args.domain, args.problem)
    except Exception as e:
        print(f"Error parsing PDDL files: {e}")
        sys.exit(1)

    # Solve
    result = solve_problem(problem, args)
    if not result:
        sys.exit(1)

    # Print result
    print(f"Status: {result.status}")
    if result.plan and hasattr(result.plan, 'actions'):
        print(f"Plan found with {len(result.plan.actions)} actions")
        for i, action in enumerate(result.plan.actions):
            params = ", ".join(map(str, action.actual_parameters))
            print(f"  {i+1}. {action.action.name}({params})")

        if args.output_plan:
            with open(args.output_plan, 'w') as f:
                for i, action in enumerate(result.plan.actions):
                    params = ", ".join(map(str, action.actual_parameters))
                    f.write(f"{i+1}. {action.action.name}({params})\n")
            print(f"Plan saved to: {args.output_plan}")
    else:
        print("No plan found")
        sys.exit(1)


if __name__ == "__main__":
    main()