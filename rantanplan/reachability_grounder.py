"""
FDI-style smart grounding via numeric abstraction and FD reachability.

Orchestrates: abstract numeric problem → FD reachability analysis →
extract reachable parameter bindings → ground original numeric problem
using only the reachable bindings.

Requires the optional ``up-fast-downward`` package.  Falls back to the naive
UP Grounder when that package is not installed.
"""

import sys
from typing import Any, Dict, List, Optional, Tuple

import unified_planning as up
from unified_planning.engines import CompilationKind
from unified_planning.engines.compilers.grounder import Grounder
from unified_planning.engines.results import CompilerResult
from unified_planning.model import Action, FNode, Problem
from unified_planning.plans.plan import ActionInstance

from .numeric_abstractor import NumericAbstractor


def _fd_available() -> bool:
    """Return True if the up-fast-downward package is importable."""
    try:
        import up_fast_downward  # noqa: F401

        return True
    except ImportError:
        return False


def _has_numeric_features(problem: Problem) -> bool:
    for fluent in problem.fluents:
        if fluent.type.is_int_type() or fluent.type.is_real_type():
            return True
    return False


def _naive_estimate(problem: Problem) -> int:
    """Cheap upper-bound on the number of ground actions the naive grounder produces."""
    total = 0
    for action in problem.actions:
        count = 1
        for param in action.parameters:
            n = sum(1 for _ in problem.objects(param.type))
            count *= max(n, 1)
        total += count
    return total


class ReachabilityGrounder:
    """
    Smart grounder using FD reachability analysis with numeric abstraction.

    For purely classical problems, delegates directly to
    ``FastDownwardReachabilityGrounder``.  For numeric problems, abstracts
    the numerics into boolean proxy predicates, runs FD reachability on
    the abstraction, then grounds the *original* numeric problem using
    only the reachable parameter bindings.
    """

    def ground(
        self,
        problem: Problem,
        compilation_kind: CompilationKind = CompilationKind.GROUNDING,
    ) -> CompilerResult:
        """
        Ground *problem*, restricting to reachable parameter bindings.

        Falls back to the naive Grounder when ``up-fast-downward`` is not
        installed or the problem has no numeric features (in which case FD
        can handle it directly).
        """
        if not _fd_available():
            print(
                "  [grounding] up-fast-downward not installed — "
                "falling back to naive grounding",
                file=sys.stderr,
            )
            return Grounder().compile(problem, compilation_kind)

        from up_fast_downward import FastDownwardReachabilityGrounder

        # Pure classical: let FD handle it directly.
        if not _has_numeric_features(problem):
            print("  [grounding] Classical problem — using FD reachability directly")
            fd_grounder = FastDownwardReachabilityGrounder()
            return fd_grounder.compile(problem, compilation_kind)

        # ---- FDI: abstract → FD → map → ground ----

        naive_est = _naive_estimate(problem)
        print(
            f"  [grounding] Numeric problem — applying FDI abstraction "
            f"(naive estimate: ~{naive_est} ground actions)"
        )

        # 1. Abstract numeric features into boolean proxies.
        abstractor = NumericAbstractor()
        abstracted_problem, fluent_name_map = abstractor.abstract(problem)
        num_numeric = len(fluent_name_map)
        print(
            f"  [grounding] Abstraction: {len(problem.actions)} action schemas, "
            f"{num_numeric} numeric fluents abstracted"
        )

        # 2. Ground the abstraction with FD reachability.
        fd_grounder = FastDownwardReachabilityGrounder()
        try:
            fd_result = fd_grounder.compile(
                abstracted_problem, CompilationKind.GROUNDING
            )
        except Exception as e:
            print(
                f"  [grounding] FD reachability failed ({e}) — "
                f"falling back to naive grounding",
                file=sys.stderr,
            )
            return Grounder().compile(problem, compilation_kind)

        grounded_abstracted = fd_result.problem
        print(
            f"  [grounding] FD reachability: "
            f"{len(grounded_abstracted.actions)} reachable ground actions"
        )

        # 3. Extract reachable (action-name, param-tuple) pairs and build map.
        grounding_map = self._build_grounding_map(
            problem, grounded_abstracted, fd_result
        )
        total_reachable = sum(len(v) for v in grounding_map.values())
        print(
            f"  [grounding] Grounding map: {total_reachable} reachable bindings "
            f"(~{max(1, naive_est // max(1, total_reachable))}× reduction)"
        )

        # 4. Ground the ORIGINAL problem using only the reachable bindings.
        numeric_grounder = Grounder(grounding_actions_map=grounding_map)
        return numeric_grounder.compile(problem, compilation_kind)

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    @staticmethod
    def _build_grounding_map(
        original_problem: Problem,
        grounded_abstracted: Problem,
        fd_result: CompilerResult,
    ) -> Dict[Action, List[Tuple[FNode, ...]]]:
        """
        Map reachable parameter bindings from the grounded abstraction back
        to the original problem's lifted actions.
        """
        em = original_problem.environment.expression_manager
        actions_map: Dict[Action, List[Tuple[FNode, ...]]] = {}

        for grounded_action in grounded_abstracted.actions:
            # Use the FD result's map-back to get the lifted action & params.
            ai = ActionInstance(grounded_action)
            lifted_ai = fd_result.map_back_action_instance(ai)

            # The lifted action belongs to the *abstracted* problem.
            # Look up the corresponding action in the *original* problem.
            try:
                original_action = original_problem.action(lifted_ai.action.name)
            except Exception:
                # Action present in abstraction but not original (shouldn't happen).
                continue

            # The parameters are ObjectExp FNodes — valid in both problems
            # because they share the same Environment and Objects.
            params = tuple(lifted_ai.actual_parameters)

            if original_action not in actions_map:
                actions_map[original_action] = []
            actions_map[original_action].append(params)

        return actions_map
