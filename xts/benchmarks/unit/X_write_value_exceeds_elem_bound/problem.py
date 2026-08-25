"""Writing constant values that exceed array element type bounds (99 and -7). PDDL-XTS: X_write_value_exceeds_elem_bound"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_write_value_exceeds_elem_bound')

    slot_t = IntType(0, 3)
    val_t  = IntType(0, 5)

    cells = Fluent('cells', ArrayType(4, val_t))
    p.add_fluent(cells)
    p.set_initial_value(cells, [0, 1, 2, 3])

    # Error: Int(99) > ub=5 — value exceeds element type upper bound
    set_max = InstantaneousAction('set_max', i=slot_t)
    i = set_max.parameter('i')
    set_max.add_effect(cells[i], Int(99))
    p.add_action(set_max)

    # Error: Int(-7) < lb=0 — value below element type lower bound
    set_neg = InstantaneousAction('set_neg', i=slot_t)
    i2 = set_neg.parameter('i')
    set_neg.add_effect(cells[i2], Int(-7))
    p.add_action(set_neg)

    p.add_goal(Equals(cells[Int(0)], 99))
    return p