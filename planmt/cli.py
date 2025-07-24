#!/usr/bin/env python3
"""
planMT CLI - Command Line Interface for the planMT planning system.

This CLI provides an interface to the planMT planner, which uses a planning-as-satisfiability 
approach with a C++ backend and Unified Planning frontend.
"""

import sys
from planmt.cli_modules.args import parse_arguments, validate_files
from planmt.cli_modules.solver import parse_problem, solve_problem
from planmt.cli_modules.output import print_result


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
