"""Writing a set-valued expression into an integer array cell — type mismatch. PDDL-XTS: X_write_set_into_int_cell"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_set_into_int')
    val_t = IntType(0, 9)

    cells = Fluent('cells', ArrayType(3, val_t))
    bag   = Fluent('bag',   SetType(val_t))
    p.add_fluent(cells)
    p.add_fluent(bag, default_initial_value=set())
    p.set_initial_value(cells, [0, 0, 0])
    p.set_initial_value(bag,   {Int(1), Int(2), Int(3)})

    # Error: cells[0] expects an integer, but bag is a set
    overwrite = InstantaneousAction('overwrite')
    overwrite.add_effect(cells[0], bag)
    p.add_action(overwrite)

    p.add_goal(Equals(cells[0], 1))
    return p
