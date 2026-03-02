#!/usr/bin/env python3
"""Quick smoke test for the numeric abstractor and reachability grounder."""

import sys
import traceback

sys.path.insert(0, '.')

def test_abstractor():
    """Test the NumericAbstractor on zenotravel."""
    from unified_planning.io import PDDLReader
    from rantanplan.numeric_abstractor import NumericAbstractor

    reader = PDDLReader()
    problem = reader.parse_problem(
        'pddl/test/zenotravel/domain.pddl',
        'pddl/test/zenotravel/problem.pddl'
    )

    # Check numeric features
    numeric_fluents = [f for f in problem.fluents if f.type.is_int_type() or f.type.is_real_type()]
    print(f"Problem: {problem.name}")
    print(f"  Fluents: {len(problem.fluents)} ({len(numeric_fluents)} numeric)")
    print(f"  Actions: {len(problem.actions)}")
    print(f"  Objects: {sum(1 for _ in problem.all_objects)}")

    # Test abstraction
    abstractor = NumericAbstractor()
    abstracted, fluent_map = abstractor.abstract(problem)

    print(f"\nAbstracted problem: {abstracted.name}")
    print(f"  Fluents: {len(abstracted.fluents)}")
    print(f"  Actions: {len(abstracted.actions)}")
    print(f"  Fluent map: {fluent_map}")

    # Check no numeric fluents remain
    remaining_numeric = [f for f in abstracted.fluents if f.type.is_int_type() or f.type.is_real_type()]
    assert len(remaining_numeric) == 0, f"Still have numeric fluents: {remaining_numeric}"
    print("  [OK] No numeric fluents in abstracted problem")

    # Check action count matches
    assert len(abstracted.actions) == len(problem.actions), \
        f"Action count mismatch: {len(abstracted.actions)} vs {len(problem.actions)}"
    print("  [OK] Action count preserved")

    return True


def test_reachability_grounder():
    """Test the full ReachabilityGrounder pipeline."""
    from unified_planning.io import PDDLReader
    from unified_planning.engines import CompilationKind
    from rantanplan.reachability_grounder import ReachabilityGrounder

    reader = PDDLReader()
    problem = reader.parse_problem(
        'pddl/test/zenotravel/domain.pddl',
        'pddl/test/zenotravel/problem.pddl'
    )

    # Initialize fluents (as the main pipeline does)
    from unified_planning.shortcuts import get_environment, Fraction
    _env = get_environment()
    _tm = _env.type_manager
    _em = _env.expression_manager
    from unified_planning.model.fluent import get_all_fluent_exp

    problem.initial_defaults.update({_tm.RealType(): _em.Real(Fraction(0))})
    problem.initial_defaults.update({_tm.IntType(): _em.Int(0)})
    problem.initial_defaults.update({_tm.BoolType(): _em.Bool(False)})

    all_fe = []
    for fluent in problem.fluents:
        all_fe.extend(list(get_all_fluent_exp(problem, fluent)))
    init_keys = list(problem.explicit_initial_values.keys())
    for fe in all_fe:
        if fe not in init_keys:
            problem.set_initial_value(fe, problem.initial_defaults[fe.type])

    print("\nTesting ReachabilityGrounder...")
    grounder = ReachabilityGrounder()
    result = grounder.ground(problem, CompilationKind.GROUNDING)

    grounded_problem = result.problem
    print(f"  Grounded actions: {len(grounded_problem.actions)}")
    print("  [OK] ReachabilityGrounder completed successfully")

    return True


if __name__ == '__main__':
    try:
        ok1 = test_abstractor()
        ok2 = test_reachability_grounder()
        if ok1 and ok2:
            print("\n=== ALL TESTS PASSED ===")
        else:
            print("\n=== SOME TESTS FAILED ===")
            sys.exit(1)
    except Exception as e:
        print(f"\n=== ERROR ===\n{e}")
        traceback.print_exc()
        sys.exit(1)
