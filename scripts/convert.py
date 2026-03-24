#!/usr/bin/env python3

import argparse
import sys
import os

import unified_planning as up
from unified_planning.shortcuts import Fraction
from unified_planning.grpc.proto_writer import ProtobufWriter
from unified_planning.engines.compilers import Grounder, QuantifiersRemover
from unified_planning.engines import CompilationKind
from unified_planning.shortcuts import get_environment
from unified_planning.model.fluent import get_all_fluent_exp
from unified_planning.io import PDDLReader

def initialize_fluents(task):
    """Initialize uninitialized fluents with default values."""
    _env = get_environment()
    _tm = _env.type_manager
    _em = _env.expression_manager
    task.initial_defaults.update({_tm.RealType(): _em.Real(Fraction(0))})
    task.initial_defaults.update({_tm.IntType(): _em.Int(0)})
    task.initial_defaults.update({_tm.BoolType(): _em.Bool(False)})

    # Get all fluent expressions and find uninitialized ones
    all_fluent_expressions = []
    for fluent in task.fluents:
        fluent_expressions = list(get_all_fluent_exp(task, fluent))
        all_fluent_expressions.extend(fluent_expressions)

    initialized_fluents = list(task.explicit_initial_values.keys())
    uninitialized_fluents = [f for f in all_fluent_expressions if f not in initialized_fluents]

    # Initialize uninitialized fluents
    for fe in uninitialized_fluents:
        task.set_initial_value(fe, task.initial_defaults[fe.type])

def compile_problem(problem, grounding_mode='up'):
    """Apply compilation pipeline (quantifier removal + optional grounding).

    Args:
        problem: The UP problem to compile.
        grounding_mode: 'up' (cross-product via UP), or 'none' (lifted).
    """
    current_problem = problem

    # Step 1: Remove quantifiers if present
    quantifier_remover = QuantifiersRemover()
    if quantifier_remover.supports(current_problem.kind):
        print("Applying quantifier removal...")
        quantifier_result = quantifier_remover.compile(current_problem, CompilationKind.QUANTIFIERS_REMOVING)
        current_problem = quantifier_result.problem

    if grounding_mode == 'none':
        print("Skipping grounding (lifted output).")
        return current_problem

    # Step 2: Ground the problem using UP's cross-product grounder
    print("Applying UP grounding (cross-product)...")
    grounder = Grounder()
    grounding_result = grounder.compile(current_problem, CompilationKind.GROUNDING)
    current_problem = grounding_result.problem

    return current_problem

def main():
    parser = argparse.ArgumentParser(description='Convert PDDL domain and problem files to protobuf format')
    parser.add_argument('-d', '--domain', required=True, help='Path to PDDL domain file')
    parser.add_argument('-p', '--problem', required=True, help='Path to PDDL problem file')
    parser.add_argument('-o', '--output', required=True, help='Output protobuf file path')
    grounding = parser.add_mutually_exclusive_group()
    grounding.add_argument('--up-grounding', action='store_true',
                           help='Use UP cross-product grounding (default)')
    grounding.add_argument('--no-grounding', action='store_true',
                           help='Skip grounding entirely — output a lifted protobuf (for C++ grounder)')

    args = parser.parse_args()

    # Determine grounding mode
    if args.no_grounding:
        grounding_mode = 'none'
    else:
        grounding_mode = 'up'

    # Check input files exist
    if not os.path.exists(args.domain):
        print(f"Error: Domain file '{args.domain}' not found")
        sys.exit(1)

    if not os.path.exists(args.problem):
        print(f"Error: Problem file '{args.problem}' not found")
        sys.exit(1)

    try:
        # Read PDDL files
        print(f"Reading domain: {args.domain}")
        print(f"Reading problem: {args.problem}")

        reader = PDDLReader()
        problem = reader.parse_problem(args.domain, args.problem)

        # Initialize fluents
        print("Initializing fluents...")
        initialize_fluents(problem)

        # Compile problem
        print("Compiling problem...")
        compiled_problem = compile_problem(problem, grounding_mode=grounding_mode)

        # Convert to protobuf
        print("Converting to protobuf...")
        writer = ProtobufWriter()
        pb_problem_msg = writer.convert(compiled_problem)

        # Write to output file
        print(f"Writing to: {args.output}")
        with open(args.output, "wb") as f:
            f.write(pb_problem_msg.SerializeToString())

        print("Conversion completed successfully!")

    except Exception as e:
        print(f"Error during conversion: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()