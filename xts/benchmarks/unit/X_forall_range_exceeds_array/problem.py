"""Forall range variable exceeds array bounds (indices 4..7 on size-4 array). PDDL-XTS: X_forall_range_exceeds_array"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_forall_range_exceeds_array')

    val_t = IntType(0, 9)

    cells = Fluent('cells', ArrayType(4, val_t))
    p.add_fluent(cells)
    p.set_initial_value(cells, [5, 3, 8, 1])

    # Error: RangeVariable 0..7 covers indices 4..7 which are OOB for size-4 array
    zero_all = InstantaneousAction('zero_all')
    rv = RangeVariable('i', 0, 7)
    zero_all.add_effect(cells[rv], 0, forall=[rv])
    p.add_action(zero_all)

    p.add_goal(Equals(cells[Int(0)], 7))
    return p