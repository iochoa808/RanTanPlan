"""Array initialisation with more elements than the declared size. PDDL-XTS: X_array_mk_overcount"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_array_mk_overcount')

    val_t = IntType(0, 9)

    cells = Fluent('cells', ArrayType(3, val_t))
    p.add_fluent(cells)

    # Error: 5 elements provided for a size-3 array
    p.set_initial_value(cells, [0, 1, 2, 3, 4])

    bump = InstantaneousAction('bump')
    bump.add_effect(cells[Int(0)], Plus(cells[Int(0)], 1))
    p.add_action(bump)

    p.add_goal(Equals(cells[Int(0)], 1))
    return p