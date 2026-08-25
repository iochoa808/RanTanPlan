"""Two conflicting effects write to the same array cell in one action. PDDL-XTS: X_double_write_same_cell"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_double_write_same_cell')

    val_t = IntType(0, 9)

    cells = Fluent('cells', ArrayType(3, val_t))
    p.add_fluent(cells)
    p.set_initial_value(cells, [1, 5, 2])

    # Error: two constant writes to cells[0] in the same action
    overwrite_const = InstantaneousAction('overwrite_const')
    overwrite_const.add_effect(cells[Int(0)], 3)
    overwrite_const.add_effect(cells[Int(0)], 7)
    p.add_action(overwrite_const)

    # Error: two arithmetic writes to cells[1] in the same action
    overwrite_read_arith = InstantaneousAction('overwrite_read_arith')
    overwrite_read_arith.add_effect(cells[Int(1)], Plus(cells[Int(1)], 1))
    overwrite_read_arith.add_effect(cells[Int(1)], Plus(cells[Int(1)], 2))
    p.add_action(overwrite_read_arith)

    p.add_goal(Equals(cells[Int(1)], 7))
    return p