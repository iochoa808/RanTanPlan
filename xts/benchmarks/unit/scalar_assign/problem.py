"""
Scalar copy — (assign (a) (b)) between two plain bounded-int fluents.
Initial: a=0, b=7; goal: a=7.
Plan: copy()  →  a=7.
PDDL-XTS: scalar_assign
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('scalar_copy')
    score_t = IntType(0, 9)

    a = Fluent('a', score_t)
    b = Fluent('b', score_t)
    p.add_fluent(a, default_initial_value=0)
    p.add_fluent(b, default_initial_value=0)
    p.set_initial_value(a, 0)
    p.set_initial_value(b, 7)

    copy = InstantaneousAction('copy')
    copy.add_precondition(LT(a, b))
    copy.add_effect(a, b)
    p.add_action(copy)

    p.add_goal(Equals(a, 7))
    return p
