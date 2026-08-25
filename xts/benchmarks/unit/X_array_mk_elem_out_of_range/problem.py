"""Array initialisation with elements outside element type bounds. PDDL-XTS: X_array_mk_elem_out_of_range"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_array_mk_elem_out_of_range')

    val_t = IntType(0, 5)

    cells = Fluent('cells', ArrayType(3, val_t))
    p.add_fluent(cells)

    # Error: 99 > 5 and -2 < 0 — both out of range for IntType(0,5)
    p.set_initial_value(cells, [99, -2, 0])

    bump = InstantaneousAction('bump')
    bump.add_effect(cells[Int(2)], 1)
    p.add_action(bump)

    p.add_goal(Equals(cells[Int(0)], 99))
    return p