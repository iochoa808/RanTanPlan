"""
Forall goal — universal quantifier inside the goal condition.
PDDL-XTS: forall_goal
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('forall_goal')

    idx_t = IntType(0, 2)
    val_t = IntType(0, 1)

    arr = Fluent('arr', ArrayType(3, val_t))
    p.add_fluent(arr)
    p.set_initial_value(arr, [0, 0, 0])

    fill = InstantaneousAction('fill', i=idx_t)
    i = fill.parameter('i')
    fill.add_precondition(Equals(arr[i], 0))
    fill.add_effect(arr[i], 1)
    p.add_action(fill)

    em = get_environment().expression_manager
    rv = RangeVariable('i', 0, 2)
    p.add_goal(Forall(Equals(em.ArrayRead(em.FluentExp(arr), em.RangeVariableExp(rv)), 1), rv))
    return p
