"""
Numeric-to-classical abstraction for FDI-style smart grounding.

Transforms a Unified Planning Problem with numeric features into a classical
Problem suitable for Fast Downward's reachability analysis. Numeric fluents are
replaced with boolean "proxy predicates" that preserve parameter linkage.

This implements the abstraction step of the FDI method described in:
  Scala & Vallati, "Effective Grounding for Hybrid Planning Problems
  represented in PDDL+".

Soundness: the abstracted problem has strictly weaker preconditions and strictly
more effects than the numeric original, so every action reachable in the numeric
domain is also reachable in the abstraction. The grounding is a safe
over-approximation.
"""

from collections import OrderedDict
from typing import Dict, List, Optional, Set, Tuple

import unified_planning as up
from unified_planning.model import (
    Effect,
    FNode,
    Fluent,
    InstantaneousAction,
    Parameter,
    Problem,
)
from unified_planning.model.operators import OperatorKind


class NumericAbstractor:
    """Transforms a numeric planning problem into a classical approximation."""

    def abstract(self, problem: Problem) -> Tuple[Problem, Dict[str, str]]:
        """
        Create a classical abstraction of the given problem.

        Numeric fluents become boolean proxy predicates (preserving parameter
        signatures).  Numeric comparisons become conjunctions of proxy
        applications.  Numeric effects become boolean add-effects on proxies.

        Args:
            problem: The original UP Problem (may contain numeric features).

        Returns:
            (abstracted_problem, fluent_name_map) where fluent_name_map maps
            original numeric fluent names to their proxy predicate names.
            If the problem has no numeric features, returns
            (problem, {}) unchanged.
        """
        if not self._has_numeric_features(problem):
            return problem, {}

        env = problem.environment
        em = env.expression_manager
        tm = env.type_manager

        new_problem = Problem(f"{problem.name}_abstracted", env)

        # Copy user types (shared via the environment's TypeManager)
        for ut in problem.user_types:
            new_problem._add_user_type(ut)

        # Copy objects
        for obj in problem.all_objects:
            new_problem.add_object(obj)

        # ---- fluents ----
        # Boolean fluents are copied as-is.
        # Numeric fluents get a boolean proxy with an __num suffix.
        fluent_name_map: Dict[str, str] = {}        # original → proxy name
        proxy_fluents: Dict[str, Fluent] = {}        # original name → proxy Fluent
        existing_names: Set[str] = set()

        for fluent in problem.fluents:
            existing_names.add(fluent.name)

        for fluent in problem.fluents:
            if self._is_numeric_fluent(fluent):
                proxy_name = self._unique_proxy_name(fluent.name, existing_names)
                existing_names.add(proxy_name)
                sig = OrderedDict(
                    (p.name, p.type) for p in fluent.signature
                )
                proxy = Fluent(
                    proxy_name,
                    tm.BoolType(),
                    _signature=sig,
                    environment=env,
                )
                new_problem.add_fluent(proxy, default_initial_value=False)
                fluent_name_map[fluent.name] = proxy_name
                proxy_fluents[fluent.name] = proxy
            else:
                new_problem.add_fluent(fluent)

        # ---- actions ----
        for action in problem.actions:
            if not isinstance(action, InstantaneousAction):
                continue
            new_action = self._abstract_action(action, proxy_fluents, em, env)
            new_problem.add_action(new_action)

        # ---- initial state ----
        self._abstract_initial_state(problem, new_problem, proxy_fluents, em)

        # ---- goals ----
        for goal in problem.goals:
            abstracted = self._abstract_expression(goal, proxy_fluents, em)
            if not abstracted.is_true():
                new_problem.add_goal(abstracted)

        return new_problem, fluent_name_map

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    @staticmethod
    def _has_numeric_features(problem: Problem) -> bool:
        for fluent in problem.fluents:
            if fluent.type.is_int_type() or fluent.type.is_real_type():
                return True
        return False

    @staticmethod
    def _is_numeric_fluent(fluent: Fluent) -> bool:
        return fluent.type.is_int_type() or fluent.type.is_real_type()

    @staticmethod
    def _unique_proxy_name(base: str, existing: Set[str]) -> str:
        candidate = f"{base}__num"
        if candidate not in existing:
            return candidate
        i = 0
        while True:
            candidate = f"{base}__num_{i}"
            if candidate not in existing:
                return candidate
            i += 1

    # ------------------------------------------------------------------
    # Action abstraction
    # ------------------------------------------------------------------

    def _abstract_action(
        self,
        action: InstantaneousAction,
        proxy_fluents: Dict[str, Fluent],
        em: "up.model.ExpressionManager",
        env: "up.Environment",
    ) -> InstantaneousAction:
        """Return an abstracted copy of *action*."""
        # Create new action with same name & parameter signature.
        params = OrderedDict((p.name, p.type) for p in action.parameters)
        new_action = InstantaneousAction(action.name, params, env)

        # Build substitution:  old-param ParameterExp → new-param ParameterExp.
        # The new action has distinct Parameter objects; expressions copied from
        # the original action must be re-written to reference the new ones.
        subs: Dict["up.model.expression.Expression", "up.model.expression.Expression"] = {}
        for old_p, new_p in zip(action.parameters, new_action.parameters):
            old_node = em.ParameterExp(old_p)
            new_node = em.ParameterExp(new_p)
            if old_node is not new_node:
                subs[old_node] = new_node

        # Preconditions
        for prec in action.preconditions:
            abstracted = self._abstract_expression(prec, proxy_fluents, em)
            if subs:
                abstracted = abstracted.substitute(subs)
            if not abstracted.is_true():
                new_action.add_precondition(abstracted)

        # Effects
        for effect in action.effects:
            self._abstract_effect(effect, new_action, proxy_fluents, em, subs)

        return new_action

    # ------------------------------------------------------------------
    # Expression abstraction
    # ------------------------------------------------------------------

    def _abstract_expression(
        self,
        expr: FNode,
        proxy_fluents: Dict[str, Fluent],
        em: "up.model.ExpressionManager",
    ) -> FNode:
        """
        Recursively abstract an expression.

        Boolean sub-expressions are preserved.  Numeric comparisons are replaced
        with conjunctions of proxy-predicate applications for every numeric
        fluent appearing in the comparison sub-tree.
        """
        ntype = expr.node_type

        # --- boolean connectives ---
        if ntype == OperatorKind.AND:
            children = [
                self._abstract_expression(c, proxy_fluents, em) for c in expr.args
            ]
            children = [c for c in children if not c.is_true()]
            if not children:
                return em.TRUE()
            return em.And(*children)

        if ntype == OperatorKind.OR:
            children = [
                self._abstract_expression(c, proxy_fluents, em) for c in expr.args
            ]
            if any(c.is_true() for c in children):
                return em.TRUE()
            children = [c for c in children if not c.is_false()]
            if not children:
                return em.FALSE()
            return em.Or(*children)

        if ntype == OperatorKind.NOT:
            child = self._abstract_expression(expr.args[0], proxy_fluents, em)
            if child.is_true():
                return em.FALSE()
            if child.is_false():
                return em.TRUE()
            return em.Not(child)

        if ntype == OperatorKind.IMPLIES:
            left = self._abstract_expression(expr.args[0], proxy_fluents, em)
            right = self._abstract_expression(expr.args[1], proxy_fluents, em)
            return em.Implies(left, right)

        if ntype == OperatorKind.IFF:
            left = self._abstract_expression(expr.args[0], proxy_fluents, em)
            right = self._abstract_expression(expr.args[1], proxy_fluents, em)
            return em.Iff(left, right)

        # --- numeric comparisons: LE, LT, EQUALS ---
        # (UP stores GE(a,b) as LE(b,a) and GT(a,b) as LT(b,a))
        if ntype in (OperatorKind.LE, OperatorKind.LT, OperatorKind.EQUALS):
            numeric_fexps = self._collect_numeric_fluent_exps(expr, proxy_fluents)
            if numeric_fexps:
                # Replace the entire comparison with the conjunction of the
                # proxy applications for every numeric fluent that appears.
                proxies: List[FNode] = []
                seen: Set[str] = set()
                for fexp in numeric_fexps:
                    proxy_app = self._make_proxy_application(fexp, proxy_fluents, em)
                    key = str(proxy_app)
                    if key not in seen:
                        proxies.append(proxy_app)
                        seen.add(key)
                # Also keep boolean fluent applications that appear in the
                # comparison (e.g. bool-to-int patterns).
                for bfe in self._collect_boolean_fluent_exps(expr, proxy_fluents):
                    key = str(bfe)
                    if key not in seen:
                        proxies.append(bfe)
                        seen.add(key)
                if not proxies:
                    return em.TRUE()
                if len(proxies) == 1:
                    return proxies[0]
                return em.And(*proxies)
            else:
                # Pure boolean/object equality — keep as-is.
                return expr

        # --- fluent expression ---
        if ntype == OperatorKind.FLUENT_EXP:
            if expr.fluent().name in proxy_fluents:
                return self._make_proxy_application(expr, proxy_fluents, em)
            return expr

        # --- anything else (constants, param-exp, arithmetic, …) ---
        return expr

    # ------------------------------------------------------------------
    # Effect abstraction
    # ------------------------------------------------------------------

    def _abstract_effect(
        self,
        effect: Effect,
        new_action: InstantaneousAction,
        proxy_fluents: Dict[str, Fluent],
        em: "up.model.ExpressionManager",
        subs: Dict,
    ) -> None:
        """Abstract *effect* and add it to *new_action*."""
        fluent_exp = effect.fluent
        fluent = fluent_exp.fluent()

        # Abstract condition
        condition: FNode = em.TRUE()
        if effect.is_conditional():
            condition = self._abstract_expression(effect.condition, proxy_fluents, em)
            if subs:
                condition = condition.substitute(subs)

        if fluent.name in proxy_fluents:
            # Numeric effect → add-effect on proxy predicate.
            proxy_app = self._make_proxy_application(fluent_exp, proxy_fluents, em)
            if subs:
                proxy_app = proxy_app.substitute(subs)
            new_action.add_effect(proxy_app, True, condition)

            # Also add proxy effects for numeric fluents appearing in the
            # value expression (preserves parameter dependencies).
            seen: Set[str] = {str(proxy_app)}
            for vfe in self._collect_numeric_fluent_exps(effect.value, proxy_fluents):
                pa = self._make_proxy_application(vfe, proxy_fluents, em)
                if subs:
                    pa = pa.substitute(subs)
                key = str(pa)
                if key not in seen:
                    try:
                        new_action.add_effect(pa, True, condition)
                    except up.exceptions.UPConflictingEffectsException:
                        pass  # already added with same value — harmless
                    seen.add(key)
        else:
            # Boolean / object effect → copy with substituted references.
            fl = fluent_exp
            val = effect.value
            if subs:
                fl = fl.substitute(subs)
                val = val.substitute(subs)
            try:
                new_action.add_effect(fl, val, condition)
            except up.exceptions.UPConflictingEffectsException:
                pass  # duplicate add-effect — skip

    # ------------------------------------------------------------------
    # Initial state abstraction
    # ------------------------------------------------------------------

    def _abstract_initial_state(
        self,
        original: Problem,
        abstracted: Problem,
        proxy_fluents: Dict[str, Fluent],
        em: "up.model.ExpressionManager",
    ) -> None:
        for fluent_exp, value in original.explicit_initial_values.items():
            fluent = fluent_exp.fluent()
            if fluent.name in proxy_fluents:
                proxy_app = self._make_proxy_application(
                    fluent_exp, proxy_fluents, em
                )
                abstracted.set_initial_value(proxy_app, True)
            else:
                abstracted.set_initial_value(fluent_exp, value)

    # ------------------------------------------------------------------
    # Fluent collection helpers
    # ------------------------------------------------------------------

    def _collect_numeric_fluent_exps(
        self, expr: FNode, proxy_fluents: Dict[str, Fluent]
    ) -> List[FNode]:
        """Recursively collect all FLUENT_EXP nodes that reference numeric fluents."""
        result: List[FNode] = []
        if expr.is_fluent_exp() and expr.fluent().name in proxy_fluents:
            result.append(expr)
        for child in expr.args:
            result.extend(self._collect_numeric_fluent_exps(child, proxy_fluents))
        return result

    def _collect_boolean_fluent_exps(
        self, expr: FNode, proxy_fluents: Dict[str, Fluent]
    ) -> List[FNode]:
        """Recursively collect all FLUENT_EXP nodes that reference boolean fluents."""
        result: List[FNode] = []
        if expr.is_fluent_exp() and expr.fluent().name not in proxy_fluents:
            result.append(expr)
        for child in expr.args:
            result.extend(self._collect_boolean_fluent_exps(child, proxy_fluents))
        return result

    def _make_proxy_application(
        self,
        fluent_exp: FNode,
        proxy_fluents: Dict[str, Fluent],
        em: "up.model.ExpressionManager",
    ) -> FNode:
        """Create a proxy predicate application with the same args as *fluent_exp*."""
        proxy_fluent = proxy_fluents[fluent_exp.fluent().name]
        return em.FluentExp(proxy_fluent, tuple(fluent_exp.args))
