import unified_planning as up
from unified_planning.engines import Engine
from unified_planning.engines.mixins.compiler import CompilationKind, CompilerMixin
from unified_planning.engines.results import CompilerResult
from unified_planning.model.problem_kind_versioning import LATEST_PROBLEM_KIND_VERSION
from unified_planning.model import Problem, ProblemKind, Fluent
from unified_planning.model.types import _RealType, _IntType
from unified_planning.model.walkers import FluentsSubstituter
from unified_planning.engines.compilers.utils import (
    add_invariant_condition_apply_function_to_problem_expressions,
    replace_action,
)

from typing import Dict, Optional, OrderedDict, Union, cast
from functools import partial


class BoundedTypesSimplifier(Engine, CompilerMixin):
    """
    Strips bounds from bounded integer and real types, converting them to
    plain unbounded types (e.g. ``integer[0, 15]`` -> ``integer``).

    Unlike UP's ``BoundedTypesRemover``, this does NOT add bound-check
    constraints as preconditions/goals.  The bound information is simply
    discarded so the downstream planner can treat them as regular numeric
    fluents.
    """

    def __init__(self):
        Engine.__init__(self)
        CompilerMixin.__init__(self, CompilationKind.BOUNDED_TYPES_REMOVING)

    @property
    def name(self) -> str:
        return "bounded_types_simplifier"

    @staticmethod
    def supported_kind() -> ProblemKind:
        supported_kind = ProblemKind(version=LATEST_PROBLEM_KIND_VERSION)
        supported_kind.set_problem_class("ACTION_BASED")
        supported_kind.set_typing("FLAT_TYPING")
        supported_kind.set_typing("HIERARCHICAL_TYPING")
        supported_kind.set_numbers("BOUNDED_TYPES")
        supported_kind.set_numbers("CONTINUOUS_NUMBERS")
        supported_kind.set_numbers("DISCRETE_NUMBERS")
        supported_kind.set_problem_type("SIMPLE_NUMERIC_PLANNING")
        supported_kind.set_problem_type("GENERAL_NUMERIC_PLANNING")
        supported_kind.set_fluents_type("INT_FLUENTS")
        supported_kind.set_fluents_type("REAL_FLUENTS")
        supported_kind.set_fluents_type("NUMERIC_FLUENTS")
        supported_kind.set_fluents_type("OBJECT_FLUENTS")
        supported_kind.set_initial_state("UNDEFINED_INITIAL_NUMERIC")
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
        supported_kind.set_quality_metrics("PLAN_LENGTH")
        supported_kind.set_quality_metrics("OVERSUBSCRIPTION")
        supported_kind.set_quality_metrics("MAKESPAN")
        supported_kind.set_quality_metrics("FINAL_VALUE")
        supported_kind.set_actions_cost_kind("INT_NUMBERS_IN_ACTIONS_COST")
        supported_kind.set_actions_cost_kind("REAL_NUMBERS_IN_ACTIONS_COST")
        supported_kind.set_actions_cost_kind("STATIC_FLUENTS_IN_ACTIONS_COST")
        supported_kind.set_actions_cost_kind("FLUENTS_IN_ACTIONS_COST")
        supported_kind.set_oversubscription_kind("INT_NUMBERS_IN_OVERSUBSCRIPTION")
        supported_kind.set_oversubscription_kind("REAL_NUMBERS_IN_OVERSUBSCRIPTION")
        return supported_kind

    @staticmethod
    def supports(problem_kind: ProblemKind) -> bool:
        return problem_kind <= BoundedTypesSimplifier.supported_kind()

    @staticmethod
    def supports_compilation(compilation_kind: CompilationKind) -> bool:
        return True

    @staticmethod
    def resulting_problem_kind(
        problem_kind: ProblemKind,
        compilation_kind: Optional[CompilationKind] = None,
    ) -> ProblemKind:
        new_kind = problem_kind.clone()
        if new_kind.has_bounded_types():
            new_kind.unset_numbers("BOUNDED_TYPES")
        return new_kind

    @staticmethod
    def _has_bounded_types(problem: Problem) -> bool:
        """Check if the problem contains any bounded integer or real types.

        UP's TypeManager caches types by (lower_bound, upper_bound) tuple, so
        IntType() returns the unique unbounded integer (None, None).  A bounded
        type like IntType(0, 15) is a different cached object, so a simple !=
        comparison against the unbounded type detects bounds.
        """
        tm = problem.environment.type_manager
        int_type = tm.IntType()
        real_type = tm.RealType()
        for fluent in problem.fluents:
            if fluent.type.is_int_type() and fluent.type != int_type:
                return True
            if fluent.type.is_real_type() and fluent.type != real_type:
                return True
        return False

    def _compile(self, problem, compilation_kind) -> CompilerResult:
        assert isinstance(problem, Problem)
        env = problem.environment
        tm = env.type_manager

        int_type = tm.IntType()
        real_type = tm.RealType()

        new_problem = Problem(f"{problem.name}_{self.name}", env)
        new_problem.add_objects(problem.all_objects)

        new_fluents: Dict[Fluent, Fluent] = {}
        for old_fluent in problem.fluents:
            new_fluent = None

            if old_fluent.type.is_int_type() and old_fluent.type != int_type:
                signature = OrderedDict(
                    ((p.name, p.type) for p in old_fluent.signature)
                )
                new_fluent = Fluent(old_fluent.name, int_type, signature, env)

            elif old_fluent.type.is_real_type() and old_fluent.type != real_type:
                signature = OrderedDict(
                    ((p.name, p.type) for p in old_fluent.signature)
                )
                new_fluent = Fluent(old_fluent.name, real_type, signature, env)

            if new_fluent is not None:
                new_problem.add_fluent(new_fluent)
                new_fluents[old_fluent] = new_fluent
            else:
                new_problem.add_fluent(old_fluent)

        if not new_fluents:
            # No bounded types found — return the original problem unchanged
            new_to_old = {a: a for a in problem.actions}
            return CompilerResult(
                problem, partial(replace_action, map=new_to_old), self.name
            )

        fluents_substituter = FluentsSubstituter(new_fluents, env)
        new_to_old = add_invariant_condition_apply_function_to_problem_expressions(
            problem,
            new_problem,
            None,  # no invariant condition to add
            fluents_substituter.substitute_fluents,
        )

        return CompilerResult(
            new_problem, partial(replace_action, map=new_to_old), self.name
        )
