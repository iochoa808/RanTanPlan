"""
Intersect empty — disjoint sets, compute fires via disjoint precondition.
compute: precond=disjoint(a,b), effect: result=intersect(a,b)={} AND computed=true.
Initial: a={1,3}, b={2,4} (disjoint), result={}.
Goal: computed=true  (flag set by compute; result is empty as side-effect).
Plan: compute()  →  1 step.
PDDL-XTS: intersect_empty
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('intersect_empty')
    val_t = IntType(1, 9)

    a        = Fluent('a',        SetType(val_t))
    b        = Fluent('b',        SetType(val_t))
    result   = Fluent('result',   SetType(val_t))
    computed = Fluent('computed', BoolType())
    p.add_fluent(a,        default_initial_value=set())
    p.add_fluent(b,        default_initial_value=set())
    p.add_fluent(result,   default_initial_value=set())
    p.add_fluent(computed, default_initial_value=False)

    p.set_initial_value(a,      {Int(1), Int(3)})
    p.set_initial_value(b,      {Int(2), Int(4)})
    p.set_initial_value(result, set())

    compute = InstantaneousAction('compute')
    compute.add_precondition(SetDisjoint(a, b))
    compute.add_effect(result,   SetIntersection(a, b))
    compute.add_effect(computed, True)
    p.add_action(compute)

    p.add_goal(computed)
    return p
