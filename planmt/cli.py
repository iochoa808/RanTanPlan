#!/usr/bin/env python3
"""
planMT CLI - Command Line Interface for the planMT planning system.

This CLI provides an interface to the planMT planner, which uses a planning-as-satisfiability 
approach with a C++ backend and Unified Planning frontend.
"""

import argparse
import sys
import os
from pathlib import Path
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
        default=300,
        help="Timeout for planning in seconds (default: 300)"
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


def parse_problem(domain_file, problem_file, verbose=False):
    """Parse the PDDL problem and domain files."""
    try:
        from unified_planning.io import PDDLReader
    except ImportError as e:
        print(f"Error: Failed to import unified_planning: {e}")
        print("Please ensure unified-planning is installed: pip install unified-planning")
        return None
    
    if verbose:
        print(f"Parsing domain file: {domain_file}")
        print(f"Parsing problem file: {problem_file}")
    
    try:
        reader = PDDLReader()
        problem = reader.parse_problem(domain_file, problem_file)
        
        if verbose:
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
    planner_params['verbose'] = args.verbose
    
    output_stream = sys.stdout
    
    if args.verbose:
        print(f"\n--- Starting planMT solver ---")
        print(f"Timeout: {args.timeout} seconds")
        if args.executable:
            print(f"Using executable: {args.executable}")
    
    try:
        with OneshotPlanner(name='planMT', params=planner_params) as planner:
            if not planner:
                print("Error: Could not create planMT planner. Is it properly installed?")
                return None
            
            if args.verbose:
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


def print_result(result, args):
    """Print the planning result."""
    if not result:
        return False
    
    print(f"\n--- planMT Result ---")
    print(f"Status: {result.status}")
    
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
            try:
                with open(args.output_plan, 'w') as f:
                    f.write(f"Plan for problem (status: {result.status}):\n")
                    if hasattr(result.plan, 'actions'):
                        for i, action_instance in enumerate(result.plan.actions):
                            params_str = ", ".join(map(str, action_instance.actual_parameters))
                            f.write(f"{i+1}. {action_instance.action.name}({params_str})\n")
                print(f"Plan saved to: {args.output_plan}")
            except Exception as e:
                print(f"Warning: Could not save plan to file: {e}")
    else:
        print("No plan found.")
    
    # Print log messages if verbose
    if args.verbose and result.log_messages:
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
    
    # Parse the problem
    problem = parse_problem(args.domain, args.problem, args.verbose)
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
