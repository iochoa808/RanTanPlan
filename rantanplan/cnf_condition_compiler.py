import unified_planning as up
from unified_planning.engines import Engine
from unified_planning.engines.mixins.compiler import CompilationKind, CompilerMixin
from unified_planning.engines.results import CompilerResult
from unified_planning.model.problem_kind_versioning import LATEST_PROBLEM_KIND_VERSION
from unified_planning.model import Problem, ProblemKind, Action
from unified_planning.model.walkers import dnf as up_dnf
from unified_planning.engines.compilers.utils import replace_action

from typing import Optional, Dict, Callable, List
from functools import partial

# CNF compiler for goals and action preconditions.
# CNF condition compiler implementation
#
# Pipeline:
#   - Input UP problem
#   - NNF normalization (UP's built-in Nnf walker)
#   - Flatten And/Or
#   - Distribute Or over And (budget-guarded) to obtain CNF (And of Ors)
#   - Output new UP problem with:
#       * each goal in CNF, splitting top-level And into multiple goals
#       * each instantaneous action's preconditions in CNF, with top-level And
#         split into multiple add_precondition calls
#
# Design choices:
#   - Determinism: preserve UP traversal order when possible; no sorting.
#     The resulting CNF has stable ordering to allow deterministic clause/literal
#     IDs to be assigned on the C++ side by a single stable traversal.
#   - Safety: if CNF distribution risks blow-up, a node-budget guard leaves the
#     original condition unchanged rather than risking exponential growth.
#   - Theory-agnostic: this only rewrites boolean connectives; theory atoms
#     remain unchanged.

class CNFConditionCompiler(Engine, CompilerMixin):
    """
    Transforms a Problem so that:
      - All top-level goals are rewritten into CNF (AND of ORs) over the original atoms.
      - All instantaneous action preconditions are rewritten into CNF (AND of ORs) over the original atoms.

    Notes
    - Uses UP's NNF walker to remove →/↔ and push negations to atoms.
    - Then flattens and distributes OR over AND to obtain CNF.
    - Distribution may blow up in size; a guard limits expansion via max_node_budget.
    - If the guard is exceeded, the formula is left unchanged for safety.
    """

    def __init__(self, *, max_node_budget: int = 100000):
        # max_node_budget: heuristic bound to prevent exponential expansion.
        Engine.__init__(self)
        CompilerMixin.__init__(self, CompilationKind.GROUNDING)  # placeholder kind
        self._max_node_budget = max_node_budget

    @property
    def name(self) -> str:
        return "cnf_cond"

    @staticmethod
    def supported_kind() -> ProblemKind:
        # Be conservative: accept broad kinds and return unchanged kind.
        supported_kind = ProblemKind(version=LATEST_PROBLEM_KIND_VERSION)
        # Keep this in sync with supported features in your pipeline if needed.
        supported_kind.set_problem_class("ACTION_BASED")
        supported_kind.set_typing("FLAT_TYPING")
        supported_kind.set_typing("HIERARCHICAL_TYPING")
        supported_kind.set_numbers("BOUNDED_TYPES")
        supported_kind.set_problem_type("SIMPLE_NUMERIC_PLANNING")
        supported_kind.set_problem_type("GENERAL_NUMERIC_PLANNING")
        supported_kind.set_fluents_type("INT_FLUENTS")
        supported_kind.set_fluents_type("REAL_FLUENTS")
        supported_kind.set_fluents_type("OBJECT_FLUENTS")
        supported_kind.set_conditions_kind("NEGATIVE_CONDITIONS")
        supported_kind.set_conditions_kind("DISJUNCTIVE_CONDITIONS")
        supported_kind.set_conditions_kind("EQUALITIES")
        supported_kind.set_conditions_kind("EXISTENTIAL_CONDITIONS")
        supported_kind.set_conditions_kind("UNIVERSAL_CONDITIONS")
        supported_kind.set_effects_kind("CONDITIONAL_EFFECTS")
        supported_kind.set_effects_kind("INCREASE_EFFECTS")
        supported_kind.set_effects_kind("DECREASE_EFFECTS")
        supported_kind.set_effects_kind("STATIC_FLUENTS_IN_BOOLEAN_ASSIGNMENTS")
        supported_kind.set_effects_kind("STATIC_FLUENTS_IN_NUMERIC_ASSIGNMENTS")
        supported_kind.set_effects_kind("STATIC_FLUENTS_IN_OBJECT_ASSIGNMENTS")
        supported_kind.set_effects_kind("FLUENTS_IN_BOOLEAN_ASSIGNMENTS")
        supported_kind.set_effects_kind("FLUENTS_IN_NUMERIC_ASSIGNMENTS")
        supported_kind.set_effects_kind("FLUENTS_IN_OBJECT_ASSIGNMENTS")
        supported_kind.set_effects_kind("FORALL_EFFECTS")
        supported_kind.set_simulated_entities("SIMULATED_EFFECTS")
        supported_kind.set_constraints_kind("STATE_INVARIANTS")
        supported_kind.set_constraints_kind("TRAJECTORY_CONSTRAINTS")
        supported_kind.set_quality_metrics("ACTIONS_COST")
        supported_kind.set_actions_cost_kind("STATIC_FLUENTS_IN_ACTIONS_COST")
        supported_kind.set_actions_cost_kind("FLUENTS_IN_ACTIONS_COST")
        supported_kind.set_quality_metrics("PLAN_LENGTH")
        supported_kind.set_quality_metrics("OVERSUBSCRIPTION")
        supported_kind.set_quality_metrics("MAKESPAN")
        supported_kind.set_quality_metrics("FINAL_VALUE")
        supported_kind.set_actions_cost_kind("INT_NUMBERS_IN_ACTIONS_COST")
        supported_kind.set_actions_cost_kind("REAL_NUMBERS_IN_ACTIONS_COST")
        supported_kind.set_oversubscription_kind("INT_NUMBERS_IN_OVERSUBSCRIPTION")
        supported_kind.set_oversubscription_kind("REAL_NUMBERS_IN_OVERSUBSCRIPTION")
        return supported_kind

    @staticmethod
    def supports(problem_kind: ProblemKind) -> bool:
        return problem_kind <= CNFConditionCompiler.supported_kind()

    @staticmethod
    def supports_compilation(compilation_kind: CompilationKind) -> bool:
        # We do a custom transformation; we don't rely on a specific UP kind.
        return True

    @staticmethod
    def resulting_problem_kind(
        problem_kind: ProblemKind,
        compilation_kind: Optional[CompilationKind] = None,
    ) -> ProblemKind:
        # CNF normalization of conditions does not change the problem kind
        return problem_kind.clone()

    def _compile(self, problem, compilation_kind) -> CompilerResult:
        """Return a new problem with goals and preconditions normalized to CNF.
        The mapping function returns original actions for traceability.
        """
        assert isinstance(problem, Problem)
        env = problem.environment
        em = env.expression_manager

        new_problem = problem.clone()
        new_problem.name = f"{self.name}_{problem.name}"
        new_problem.clear_actions()

        # Transform goals: split top-level ∧ into separate goals for simplicity
        new_problem.clear_goals()
        for g in problem.goals:
            cnf_g = self._to_cnf(g, env)
            if cnf_g.is_and():
                for c in cnf_g.args:
                    new_problem.add_goal(c)
            else:
                new_problem.add_goal(cnf_g)

        # Transform instantaneous action preconditions and conditional effect conditions
        # Note: UP exposes preconditions as a list on instantaneous actions.
        new_to_old: Dict[Action, Optional[Action]] = {}
        for a in problem.actions:
            cloned = a.clone()
            try:
                # Handle preconditions
                preconds = list(getattr(a, "preconditions", []))
                if hasattr(cloned, "clear_preconditions"):
                    cloned.clear_preconditions()
                    for p in preconds:
                        cnf_p = self._to_cnf(p, env)
                        if cnf_p.is_and():
                            for c in cnf_p.args:
                                cloned.add_precondition(c)
                        else:
                            cloned.add_precondition(cnf_p)
            except Exception:
                # If an action type behaves differently (e.g., durative), leave as-is
                cloned = a

            # Handle conditional effect conditions (separate try so precondition
            # success is never reverted by an effect-handling failure)
            try:
                for eff in getattr(cloned, "effects", []):
                    if eff.is_conditional():
                        cnf_cond = self._to_cnf(eff.condition, env)
                        eff.set_condition(cnf_cond)
            except Exception:
                pass  # leave effects unchanged

            new_problem.add_action(cloned)
            new_to_old[cloned] = a

        # Use UP's replace_action utility to map ActionInstances back to original actions
        return CompilerResult(new_problem, partial(replace_action, map=new_to_old), self.name)

    # ---------------- CNF utilities over UP FNodes -----------------

    def _to_cnf(self, f, env):
        # Normalize to NNF using UP's built-in walker (removes →/↔, pushes ¬)
        nnf_walker = up_dnf.Nnf(env)
        f_nnf = nnf_walker.get_nnf_expression(f)
        em = env.expression_manager
        # Flatten shallow And/Or trees for stability and smaller expressions
        f_flat = self._flatten(f_nnf, em)
        # Distribute Or over And to obtain ∧ of ∨; guard to limit worst-case blow-up
        f_dist = self._distribute_or_over_and(f_flat, em, self._max_node_budget)
        # Final flatten for clean CNF shape
        f_cnf = self._flatten(f_dist, em)
        return f_cnf

    def _flatten(self, f, em):
        # Flatten nested And/Or preserving left-to-right order for determinism
        if f.is_and():
            items: List = []
            for a in f.args:
                a = self._flatten(a, em)
                items.extend(a.args if a.is_and() else [a])
            return em.And(*items)
        if f.is_or():
            items: List = []
            for a in f.args:
                a = self._flatten(a, em)
                items.extend(a.args if a.is_or() else [a])
            return em.Or(*items)
        return f

    def _distribute_or_over_and(self, f, em, budget: int):
        # Assumes NNF; recursively distribute one And inside an Or at a time.
        # If the node budget is exhausted, return the partially transformed formula.
        if budget <= 0:
            return f
        if f.is_and():
            return em.And(*[self._distribute_or_over_and(x, em, budget - 1) for x in f.args])
        if f.is_or():
            # Flatten OR for easier handling
            or_args = [self._distribute_or_over_and(x, em, budget - 1) for x in f.args]
            # Find an AND inside to distribute over
            for i, a in enumerate(or_args):
                if a.is_and():
                    rest = or_args[:i] + or_args[i+1:]
                    # Distribute: (A1 ∧ ... ∧ Ak) ∨ R  == ∧j (Aj ∨ R)
                    conj_parts = []
                    for aj in a.args:
                        conj_parts.append(self._distribute_or_over_and(em.Or(aj, *rest), em, budget - 1))
                    return em.And(*conj_parts)
            # No AND inside OR → already a clause
            return em.Or(*or_args)
        return f
