"""
Pancake Sorting — small 4-pancake instance.

Features: parameterized bounded-int fluent (val), forall-effects with RangeVariable.
Adapted from ~/unified-planning/docs/extensions/domains/pancake-sorting/PancakeSorting.py

Encoding matches the PDDL-XTS pancake_bounded domain: val(idx) rather than ArrayType,
so the RPG can correctly enumerate achievable values per-position.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))

from unified_planning.shortcuts import *


def get_problem():
    instance = [3, 1, 2, 0]
    n = len(instance)
    lo, hi = 0, n - 1

    p = Problem('pancake')
    val = Fluent('val', IntType(lo, hi), idx=IntType(lo, hi))
    p.add_fluent(val, default_initial_value=lo)
    for i in range(n):
        p.set_initial_value(val(i), instance[i])

    flip = InstantaneousAction('flip', f=IntType(1, n - 1))
    f = flip.parameter('f')
    b = RangeVariable('b', 0, f)
    flip.add_effect(val(b), val(f - b), forall=[b])
    p.add_action(flip)

    for i in range(n):
        p.add_goal(Equals(val(i), i))

    return p
