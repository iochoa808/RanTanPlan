"""
Forall constant range — forall with literal bound in precondition and effect.
PDDL-XTS: forall_const_range
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('forall_const')

    idx_t = IntType(0, 3)
    val_t = IntType(0, 5)

    cell = Fluent('cell', val_t, i=idx_t)
    p.add_fluent(cell, default_initial_value=0)
    for i in range(4):
        p.set_initial_value(cell(i), 0)

    inc_all = InstantaneousAction('inc_all')
    rv = RangeVariable('i', 0, 3)
    inc_all.add_precondition(Forall(LT(cell(rv), 4), rv))
    inc_all.add_effect(cell(rv), Plus(cell(rv), 1), forall=[rv])
    p.add_action(inc_all)

    p.add_goal(Equals(cell(0), 1))
    p.add_goal(Equals(cell(3), 1))
    return p
