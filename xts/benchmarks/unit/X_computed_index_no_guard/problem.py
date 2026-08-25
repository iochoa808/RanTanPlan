"""Computed index reads OOB cell (size-1 array, reads index 1). PDDL-XTS: X_computed_index_no_guard"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_computed_index_no_guard')

    slot_t = IntType(0, 0)   # only valid index is 0
    val_t  = IntType(0, 9)

    cells = Fluent('cells', ArrayType(1, val_t))
    p.add_fluent(cells)
    p.set_initial_value(cells, [9])

    # Error/UNSOLVABLE: cells[Plus(i, 1)] reads index 1 which is OOB for size-1 array
    shift_right = InstantaneousAction('shift_right', i=slot_t)
    i = shift_right.parameter('i')
    shift_right.add_effect(cells[i], cells[Plus(i, Int(1))])
    p.add_action(shift_right)

    p.add_goal(Equals(cells[Int(0)], 5))
    return p