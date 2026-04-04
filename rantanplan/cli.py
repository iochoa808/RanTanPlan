#!/usr/bin/env python3
"""
RantanPlan CLI - Command Line Interface for the RantanPlan planning system.

This CLI provides an interface to the RantanPlan planner, which uses a planning-as-SMT
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
        description="RantanPlan - A planning-as-SMT planner",
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
    parser.add_argument("--mode",
                       choices=["satisficing", "optimal", "anytime"],
                       default=None,
                       help="Search mode: satisficing (first plan, default), "
                            "optimal (cost-optimal with proof), "
                            "anytime (keep improving, write intermediate plans). "
                            "Note: anytime mode deletes existing plan files "
                            "matching the output prefix before starting")

    # List strategies
    parser.add_argument("--list-strategies", action="store_true",
                       help="List available strategies and exit")
    
    # Optional global parameters
    parser.add_argument("--timeout", type=int, default=None,
                       help="Timeout in seconds (default: None)")
    parser.add_argument("--max-steps", type=int,
                       help="Maximum planning steps")
    parser.add_argument("--executable", type=str,
                       help="Path to RantanPlan C++ executable")
    parser.add_argument("--output-plan", type=str,
                       help="Base path for plan output in IPC format (files written as <path>.1, .2, etc.)")
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
    parser.add_argument("--symmetries", action="store_true",
                       help="Enable symmetry detection and breaking")
    parser.add_argument("--no-cnf-normalization", action="store_true",
                       help="Disable CNF normalization of goals and preconditions")
    parser.add_argument("--smt-rpg-checker", action="store_true",
                       help="Use Z3 SMT solver for RPG applicability checks instead of interval arithmetic (default: interval)")
    parser.add_argument("--no-goal-relevance", action="store_true",
                       help="Disable semantic goal-relevance analysis (enabled by default)")

    # Logging
    parser.add_argument("--log-file", type=str,
                       help="Write solver decision log to file (for use with plot.py)")

    # Horizon schedule
    parser.add_argument("--horizon-schedule",
                       choices=["linear", "arithmetic", "geometric", "doubling"],
                       default=None,
                       help="Horizon growth schedule (default: linear / one-by-one). "
                            "'doubling' doubles each batch, 'geometric' multiplies by 1.5, "
                            "'arithmetic' adds 5 each batch.")

    parser.add_argument('--up-grounding', action='store_true',
                        help='Use Unified Planning grounding instead of '
                             'the default reachability-based grounding')
    parser.add_argument('--no-numeric-grounding', action='store_true',
                        help='Disable numeric interval bounds in reachability '
                             'grounder (enabled by default)')

    parser.add_argument(
        "--version",
        action="version",
        version=f"RantanPlan {metadata.version('rantanplan')}"
    )
    
    return parser.parse_args()








def _build_planner_params(args) -> Dict[str, object]:
    """Build planner parameters dictionary from CLI arguments."""
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
    if args.symmetries:
        planner_params['symmetries'] = True
    if args.no_cnf_normalization:
        planner_params['no_cnf_normalization'] = True
    if args.smt_rpg_checker:
        planner_params['smt_rpg_checker'] = True
    if args.no_goal_relevance:
        planner_params['no_goal_relevance'] = True
    if args.horizon_schedule is not None:
        planner_params['horizon_schedule'] = args.horizon_schedule
    if args.log_file:
        planner_params['log_file'] = args.log_file
    if args.up_grounding:
        planner_params['up_grounding'] = True
    if args.no_numeric_grounding:
        planner_params['numeric_grounding'] = False
    if args.mode is not None:
        planner_params['mode'] = args.mode
    if args.output_plan:
        planner_params['output_plan'] = args.output_plan
    # Handle verbosity
    if args.silent:
        planner_params['verbosity'] = "silent"
    elif args.verbose == 1:
        planner_params['verbosity'] = "verbose"
    elif args.verbose >= 2:
        planner_params['verbosity'] = "debug"

    return planner_params


def solve_problem(problem, args):
    """Solve problem using selected strategy and mode."""
    try:
        from unified_planning.shortcuts import OneshotPlanner, AnytimePlanner
        from unified_planning.engines import PlanGenerationResultStatus
        import rantanplan
    except ImportError as e:
        print(f"Error: Failed to import dependencies: {e}")
        return None

    planner_params = _build_planner_params(args)

    # Show configuration info
    if not args.silent and args.verbose >= 1:
        print(f"Strategy: {args.strategy}")

    try:
        PlannerClass: Any = OneshotPlanner

        if args.mode == "anytime":
            with AnytimePlanner(name='RantanPlan-anytime', params=planner_params) as planner:
                last_result = None
                for result in planner.get_solutions(problem, timeout=args.timeout):
                    if result.status == PlanGenerationResultStatus.INTERMEDIATE:
                        if not args.silent and result.plan:
                            print(f"Improving plan found ({len(result.plan.actions)} actions)")
                    last_result = result
                return last_result
        elif args.mode == "optimal":
            with PlannerClass(name='RantanPlan-optimal', params=planner_params) as planner:
                result = planner.solve(problem, timeout=args.timeout)
                return result
        else:
            # satisficing (default)
            with PlannerClass(name='RantanPlan', params=planner_params) as planner:
                result = planner.solve(problem, timeout=args.timeout)
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
        import rantanplan
        executable_path = rantanplan.planner_wrapper.RantanPlanPlanner(**{})._find_executable(None)
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

    # Print result (unless --silent)
    if not args.silent:
        print(f"Status: {result.status}")
    if result.plan and hasattr(result.plan, 'actions'):
        if not args.silent:
            print(f"Plan found with {len(result.plan.actions)} actions")
            for i, action in enumerate(result.plan.actions):
                params = ", ".join(map(str, action.actual_parameters))
                print(f"  {i+1}. {action.action.name}({params})")

        # Plan files are written by the C++ backend in IPC format
    else:
        if not args.silent:
            print("No plan found")
        sys.exit(1)


if __name__ == "__main__":
    main()