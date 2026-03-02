#!/usr/bin/env python3
"""Compare naive vs smart grounding on benchmark instances."""

import sys
import time
import traceback

sys.path.insert(0, '.')

from unified_planning.io import PDDLReader
from unified_planning.engines import CompilationKind
from unified_planning.engines.compilers.grounder import Grounder
from unified_planning.shortcuts import get_environment, Fraction

def init_fluents(problem):
    """Initialize unset fluents to default values (mirrors _initialize_fluents)."""
    from unified_planning.model.fluent import get_all_fluent_exp
    env = problem.environment
    tm = env.type_manager
    em = env.expression_manager
    defaults = {tm.RealType(): em.Real(Fraction(0)), tm.IntType(): em.Int(0), tm.BoolType(): em.Bool(False)}
    all_fe = []
    for fluent in problem.fluents:
        all_fe.extend(list(get_all_fluent_exp(problem, fluent)))
    init_keys = list(problem.explicit_initial_values.keys())
    for fe in all_fe:
        if fe not in init_keys:
            if fe.type in defaults:
                problem.set_initial_value(fe, defaults[fe.type])

def test_domain(domain_path, problem_path, label):
    print(f"\n{'='*60}")
    print(f"  {label}")
    print(f"  domain:  {domain_path}")
    print(f"  problem: {problem_path}")
    print(f"{'='*60}")

    reader = PDDLReader()
    problem = reader.parse_problem(domain_path, problem_path)
    init_fluents(problem)

    numeric = [f for f in problem.fluents if f.type.is_int_type() or f.type.is_real_type()]
    print(f"  Fluents: {len(problem.fluents)} ({len(numeric)} numeric)")
    print(f"  Actions: {len(problem.actions)}")
    print(f"  Objects: {sum(1 for _ in problem.all_objects)}")

    # Naive grounding
    print(f"\n  --- Naive grounding ---")
    t0 = time.time()
    naive_result = Grounder().compile(problem, CompilationKind.GROUNDING)
    t_naive = time.time() - t0
    n_naive = len(naive_result.problem.actions)
    print(f"    Ground actions: {n_naive}")
    print(f"    Time: {t_naive:.2f}s")

    # Smart grounding
    print(f"\n  --- Smart grounding (FDI) ---")
    from rantanplan.reachability_grounder import ReachabilityGrounder
    t0 = time.time()
    smart_result = ReachabilityGrounder().ground(problem, CompilationKind.GROUNDING)
    t_smart = time.time() - t0
    n_smart = len(smart_result.problem.actions)
    print(f"    Ground actions: {n_smart}")
    print(f"    Time: {t_smart:.2f}s")

    if n_naive > 0:
        ratio = n_naive / max(n_smart, 1)
        print(f"\n  Reduction: {n_naive} -> {n_smart} ({ratio:.1f}x)")

    # Sanity: smart should never produce MORE actions
    assert n_smart <= n_naive, f"Smart produced more actions: {n_smart} vs {n_naive}"
    print(f"  [OK] Smart <= Naive")


if __name__ == '__main__':
    try:
        # Small test domain
        test_domain(
            'pddl/test/zenotravel/domain.pddl',
            'pddl/test/zenotravel/problem.pddl',
            'Zenotravel (test)'
        )

        # Rover test
        test_domain(
            'pddl/test/rover/domain.pddl',
            'pddl/test/rover/problem.pddl',
            'Rover (test)'
        )

        # Larger benchmark
        test_domain(
            'pddl/bench/rover/domain.pddl',
            'pddl/bench/rover/instances/pfile16.pddl',
            'Rover pfile16 (bench)'
        )

        print(f"\n{'='*60}")
        print("  ALL COMPARISONS PASSED")
        print(f"{'='*60}")

    except Exception as e:
        print(f"\n=== ERROR ===")
        traceback.print_exc()
        sys.exit(1)
