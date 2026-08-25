"""Constant array index out-of-bounds (size 4, index 4 or 7). PDDL-XTS: X_array_const_index_oob"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_array_const_index_oob')

    val_t = IntType(0, 9)

    cells  = Fluent('cells',  ArrayType(4, val_t))
    result = Fluent('result', val_t)
    p.add_fluent(cells)
    p.add_fluent(result, default_initial_value=0)
    p.set_initial_value(cells, [1, 2, 3, 4])

    # Error: index 4 is out of bounds for size-4 array (valid 0..3)
    read_oob = InstantaneousAction('read_oob')
    read_oob.add_effect(result, cells[Int(4)])
    p.add_action(read_oob)

    # Error: index 7 is out of bounds
    write_oob = InstantaneousAction('write_oob')
    write_oob.add_effect(cells[Int(7)], 0)
    p.add_action(write_oob)

    p.add_goal(Equals(result, 3))
    return p