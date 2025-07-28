#!/usr/bin/env python3
"""
Comprehensive test script for the planMT planning system.

This script discovers all PDDL problems in the `pddl/` directory and runs the 
planMT planner against them with various parallelism strategies. It then validates
the resulting plans to ensure correctness.
"""
import os
import sys
import argparse
from pathlib import Path
import unified_planning as up
from unified_planning.io import PDDLReader
from unified_planning.shortcuts import OneshotPlanner, PlanValidator
from unified_planning.engines.results import PlanGenerationResultStatus
from unified_planning.engines import ValidationResultStatus

# Import the planner wrapper
from planmt.planner_wrapper import planMTPlanner

# --- Test Configuration ---

# Add directories here for the --quick test
QUICK_TEST_DIRS = [
    "pddl/zenotravel",
    "pddl/rover",
    "pddl/gripper-round-1-adl",
]

# Test configurations: (parallelism_strategy, propagator)
TEST_CONFIGURATIONS = [
    #("sequential", "null"),
    #("forall", "null"),
    ("forall", "forall"),  # Forall parallelism with forall propagator
    ("forall", "forall_on_demand"),  # Forall parallelism with the forall_on_demand propagator
    ("exists", "exists")
]

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


def find_pddl_problems(root_dir="pddl", quick_test=False):
    """
    Finds all PDDL domain/problem pairs in the specified directory.
    
    Args:
        root_dir: The directory to search for PDDL files.
        quick_test: If True, only searches in the QUICK_TEST_DIRS.

    Returns:
        A list of tuples, where each tuple contains (problem_name, domain_path, problem_path).
    """
    problem_paths = []
    search_dirs = QUICK_TEST_DIRS if quick_test else [root_dir]
    
    for search_dir in search_dirs:
        for dirpath, _, filenames in os.walk(search_dir):
            domain_file = None
            problem_file = None
            
            # Find domain and problem files in the current directory
            for filename in filenames:
                if "domain" in filename.lower() and filename.endswith(".pddl"):
                    domain_file = Path(dirpath) / filename
                elif "problem" in filename.lower() and filename.endswith(".pddl"):
                    problem_file = Path(dirpath) / filename

            if domain_file and problem_file:
                problem_name = Path(dirpath).name
                problem_paths.append((problem_name, domain_file, problem_file))
                
    if not problem_paths:
        print(f"{Colors.WARNING}Warning: No PDDL problems found in the specified directories.{Colors.ENDC}")

    return problem_paths


def run_test(problem_name, domain_file, problem_file, strategy, propagator, verbose=False):
    """
    Runs a single planning test case.
    """
    test_id = f"{problem_name} ({strategy}/{propagator})"
    print_info(f"Running test: {test_id}")

    try:
        # 1. Parse the problem
        reader = PDDLReader()
        problem = reader.parse_problem(str(domain_file), str(problem_file))

        # 2. Configure and run the planner
        planner_params = {
            'parallelism': strategy,
            'propagator': propagator,
            'verbose': verbose 
        }
        
        with OneshotPlanner(name='planMT', params=planner_params) as planner:
            result = planner.solve(problem, timeout=60)

            # 3. Check the result status
            if result.status != PlanGenerationResultStatus.SOLVED_SATISFICING:
                print_fail(f"{test_id} - Planner did not find a solution. Status: {result.status.name}")
                return False

            if result.plan is None:
                print_fail(f"{test_id} - Status was SOLVED_SATISFICING but no plan was returned.")
                return False

            # 4. Validate the plan
            validator = PlanValidator()
            validation_result = validator.validate(problem, result.plan)
            
            if validation_result.status == ValidationResultStatus.VALID:
                print_pass(f"{test_id} - Plan is valid.")
                return True
            else:
                print_fail(f"{test_id} - Plan is INVALID. Validator status: {validation_result.status.name}")
                return False

    except Exception as e:
        print_fail(f"{test_id} - An unexpected error occurred: {e}")
        return False


def main():
    """Main entry point for the test script."""
    parser = argparse.ArgumentParser(description="Test script for the planMT planner.")
    parser.add_argument(
        "--quick",
        action="store_true",
        help="Run only a small subset of tests for a quick check."
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Enable verbose output from the planner."
    )
    args = parser.parse_args()

    print_header("--- Starting planMT Test Suite ---")
    if args.quick:
        print_info("Running in --quick mode. Using a subset of problems.")

    # Find problems to test
    problems = find_pddl_problems(quick_test=args.quick)
    if not problems:
        sys.exit(1)
        
    print_info(f"Found {len(problems)} problems to test against {len(TEST_CONFIGURATIONS)} configurations.")

    total_tests = 0
    passed_tests = 0
    failed_tests = 0

    # Run tests
    for problem_name, domain_file, problem_file in problems:
        for strategy, propagator in TEST_CONFIGURATIONS:
            total_tests += 1
            if run_test(problem_name, domain_file, problem_file, strategy, propagator, args.verbose):
                passed_tests += 1
            else:
                failed_tests += 1

    # Print summary
    print_header("--- Test Summary ---")
    print(f"Total tests run: {total_tests}")
    print(f"{Colors.OKGREEN}Passed: {passed_tests}{Colors.ENDC}")
    print(f"{Colors.FAIL}Failed: {failed_tests}{Colors.ENDC}")

    if failed_tests > 0:
        sys.exit(1)
    else:
        sys.exit(0)


if __name__ == "__main__":
    # Ensure the planmt module is in the Python path
    # This allows running the script from the root directory
    sys.path.insert(0, os.path.abspath(os.path.dirname(__file__)))
    main()
