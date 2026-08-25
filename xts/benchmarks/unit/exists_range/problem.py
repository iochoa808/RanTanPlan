"""
Exists range — existential quantification over bounded-int range with array read.
PDDL-XTS: exists_range
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('exists_range')

    idx_t = IntType(0, 3)
    val_t = IntType(0, 9)

    cells      = Fluent('cells',      ArrayType(4, val_t))
    found_high = Fluent('found_high')
    p.add_fluent(cells)
    p.add_fluent(found_high, default_initial_value=False)

    p.set_initial_value(cells, [3, 7, 1, 0])

    em = get_environment().expression_manager
    detect = InstantaneousAction('detect')
    rv = RangeVariable('i', 0, 3)
    detect.add_precondition(Not(found_high))
    detect.add_precondition(Exists(GT(em.ArrayRead(em.FluentExp(cells), em.RangeVariableExp(rv)), 5), rv))
    detect.add_effect(found_high, True)
    p.add_action(detect)

    boost = InstantaneousAction('boost', i=idx_t)
    i = boost.parameter('i')
    boost.add_precondition(Not(found_high))
    boost.add_effect(cells[i], Plus(cells[i], 3))
    p.add_action(boost)

    p.add_goal(found_high)
    return p
