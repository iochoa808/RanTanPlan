"""
Pancake sorting — 5-element version (matches pancake_bounded PDDL domain).
PDDL-XTS: pancake_bounded
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    instance = [3, 1, 4, 0, 2]
    n = len(instance)
    lo, hi = 0, n - 1

    p = Problem('pancake_5')
    val = Fluent('val', IntType(lo, hi), i=IntType(lo, hi))
    p.add_fluent(val, default_initial_value=lo)
    for i in range(n):
        p.set_initial_value(val(i), instance[i])

    flip = InstantaneousAction('flip', f=IntType(lo, hi))
    f = flip.parameter('f')
    b = RangeVariable('b', 0, f)
    flip.add_effect(val(b), val(f - b), forall=[b])
    p.add_action(flip)

    for i in range(n):
        p.add_goal(Equals(val(i), i))
    return p
