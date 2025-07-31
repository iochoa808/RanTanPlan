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
from unified_planning.engines.results import LogLevel


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
        action="count",
        default=0, 
        help="Increase verbosity level (-v: verbose, -vv: debug). Default is info level."
    )
    
    parser.add_argument(
        "--silent",
        action="store_true",
        help="Suppress all output"
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
        choices=["null", "forall", "forall_on_demand", "exists"],
        default="null",
        help="Propagator strategy: null (no propagation, default), "
             "forall (forall-specific propagation with conflict detection), "
             "forall_on_demand (simplified forall propagation), "
             "or exists (exists-specific propagation with cycle detection). Default: null"
    )
    
    parser.add_argument(
        "--max-steps",
        type=int,
        help="Maximum number of planning steps (default: 100)"
    )
    
    parser.add_argument(
        "--no-persist-clauses",
        action="store_true",
        help="Disable Z3 clause persistence in propagators"
    )
    
    parser.add_argument(
        "--output-plan",
        type=str,
        help="Save the plan to a file (if found)"
    )
    
    parser.add_argument(
        "--version",
        action="version",
        version=f"planMT {metadata.version('planmt')}"
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


def validate_strategy_combination(parallelism, propagator):
    """Validate that parallelism and propagator strategies are compatible."""
    if parallelism == "forall" and (propagator not in ["forall", "forall_on_demand", "null"]):
        return f"Invalid combination: 'forall' parallelism requires 'forall', 'forall_on_demand', or 'null' propagator, got '{propagator}'"
    if parallelism == "exists" and (propagator not in ["exists", "null"]):
        return f"Invalid combination: 'exists' parallelism requires 'exists' or 'null' propagator, got '{propagator}'"
    return None


def parse_problem(domain_file, problem_file, verbose=False):
    """Parse the PDDL problem and domain files."""
    try:
        from unified_planning.io import PDDLReader
    except ImportError as e:
        print(f"Error: Failed to import unified_planning: {e}")
        print("Please ensure unified-planning is installed: pip install unified-planning")
        return None
    
    if verbose and verbose >= 1:
        print(f"Parsing domain file: {domain_file}")
        print(f"Parsing problem file: {problem_file}")
    
    try:
        reader = PDDLReader()
        problem = reader.parse_problem(domain_file, problem_file)
        
        if verbose and verbose >= 1:
            print(f"Successfully parsed problem: {problem.name}")
            print(f"Number of actions: {len(problem.actions)}")
            print(f"Number of fluents: {len(problem.fluents)}")
            print(f"Number of objects: {len(problem.all_objects)}")
            print(f"Number of goals: {len(problem.goals)}")
        
        return problem
        
    except Exception as e:
        print(f"Error: Failed to parse PDDL files: {e}")
        return None


def solve_problem(problem, args):
    """Solve the planning problem using planMT."""
    try:
        from unified_planning.shortcuts import OneshotPlanner
        # Import our planner to register it
        import planmt
    except ImportError as e:
        print(f"Error: Failed to import required dependencies: {e}")
        print("Please ensure the planmt package is properly installed.")
        return None
    
    # Prepare planner parameters (not constructor options)
    planner_params = {}
    if args.executable:
        planner_params['executable_path'] = args.executable
    
    # Only pass verbosity if explicitly set (not default)
    if args.silent:
        planner_params['verbosity'] = "silent"
    elif args.verbose > 0:
        if args.verbose == 1:
            planner_params['verbosity'] = "verbose"
        else:  # 2 or higher
            planner_params['verbosity'] = "debug"
    # If args.verbose == 0 and not silent, don't pass verbosity (use C++ default)
    
    planner_params['parallelism'] = args.parallelism
    planner_params['propagator'] = args.propagator
    
    # Only pass max_steps if explicitly provided
    if args.max_steps is not None:
        planner_params['max_steps'] = args.max_steps
    
    # Only pass no_persist_clauses if explicitly set to True
    if args.no_persist_clauses:
        planner_params['no_persist_clauses'] = True
    
    output_stream = sys.stdout
    
    # Show verbose info unless silent
    if not args.silent and args.verbose >= 1:
        print(f"\n--- Starting planMT solver ---")
        print(f"Timeout: {args.timeout} seconds")
        print(f"Parallelism strategy: {args.parallelism}")
        print(f"Propagator strategy: {args.propagator}")
        if args.max_steps is not None:
            print(f"Max steps: {args.max_steps}")
        if 'verbosity' in planner_params:
            print(f"Verbosity level: {planner_params['verbosity']}")
        if args.no_persist_clauses:
            print("Z3 clause persistence: disabled")
        if args.executable:
            print(f"Using executable: {args.executable}")
    
    try:
        with OneshotPlanner(name='planMT', params=planner_params) as planner:
            if not planner:
                print("Error: Could not create planMT planner.")
                return None
            
            if not args.silent and args.verbose >= 1:
                print(f"Successfully created planner: {planner.name}")
            
            # Solve the problem
            result = planner.solve( # type: ignore
                problem,
                output_stream=output_stream,
                timeout=args.timeout
            )
            
            return result
            
    except Exception as e:
        print(f"Error: An exception occurred during planning: {e}")
        return None


def save_plan(result, output_file):
    """Save the plan to a file."""
    try:
        with open(output_file, 'w') as f:
            f.write(f"Plan for problem (status: {result.status}):\n")
            if hasattr(result.plan, 'actions'):
                for i, action_instance in enumerate(result.plan.actions):
                    params_str = ", ".join(map(str, action_instance.actual_parameters))
                    f.write(f"{i+1}. {action_instance.action.name}({params_str})\n")
        print(f"Plan saved to: {output_file}")
    except Exception as e:
        print(f"Warning: Could not save plan to file: {e}")


def print_result(result, args):
    """Print the planning result."""
    if not result:
        return False
    
    print(f"\n--- planMT Result ---")
    print(f"Status: {result.status}")
    
    # Display memory stats prominently
    if result.log_messages:
        for msg in result.log_messages:
            if "Memory usage" in msg.message:
                print(f"Memory: {msg.message.replace('Memory usage - Current: ', '').replace(' MB', '')} MB")
                break
    
    plan_found = result.plan is not None
    
    if plan_found:
        if hasattr(result.plan, 'actions'):
            # Sequential plan
            print(f"Plan found with {len(result.plan.actions)} actions:")
            for i, action_instance in enumerate(result.plan.actions):
                params_str = ", ".join(map(str, action_instance.actual_parameters))
                print(f"  {i+1}. {action_instance.action.name}({params_str})")
        else:
            print("Plan found but format not recognized")
            print(f"Plan: {result.plan}")
        
        # Save plan to file if requested
        if args.output_plan:
            save_plan(result, args.output_plan)
    else:
        print("No plan found.")
    
    # Print log messages if verbose
    if not args.silent and args.verbose >= 1 and result.log_messages:
        print(f"\nLog Messages:")
        for msg in result.log_messages:
            if msg.level == LogLevel.ERROR:
                print(f"[ERROR] {msg.message}")
            else:
                print(msg.message)
    return plan_found


def main():
    """Main entry point."""
    args = parse_arguments()
    
    # Validate input files
    file_errors = validate_files(args.domain, args.problem)
    if file_errors:
        print("Error: Invalid input files:")
        for error in file_errors:
            print(f"  - {error}")
        sys.exit(1)
    
    # Validate strategy combination
    strategy_error = validate_strategy_combination(args.parallelism, args.propagator)
    if strategy_error:
        print(f"Error: {strategy_error}")
        sys.exit(1)
    
    # Parse the problem (pass verbosity level instead of boolean)
    problem = parse_problem(args.domain, args.problem, 0 if args.silent else args.verbose)
    if not problem:
        sys.exit(1)
    
    # Solve the problem
    result = solve_problem(problem, args)
    if not result:
        sys.exit(1)
    
    # Print results and determine exit code
    plan_found = print_result(result, args)
    
    # Exit with code 0 if plan found, 1 otherwise
    sys.exit(0 if plan_found else 1)


if __name__ == "__main__":
    main()