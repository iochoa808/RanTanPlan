#!/usr/bin/env python3
"""
Problem parsing and solving logic for planMT CLI.
"""

import sys


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
    planner_params['parallelism'] = args.parallelism
    planner_params['propagator'] = args.propagator
    
    output_stream = sys.stdout
    
    if args.verbose:
        print(f"\n--- Starting planMT solver ---")
        print(f"Timeout: {args.timeout} seconds")
        print(f"Parallelism strategy: {args.parallelism}")
        print(f"Propagator strategy: {args.propagator}")
        if args.executable:
            print(f"Using executable: {args.executable}")
    
    try:
        with OneshotPlanner(name='planMT', params=planner_params) as planner:
            if not planner:
                print("Error: Could not create planMT planner.")
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