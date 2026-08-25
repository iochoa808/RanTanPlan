"""2D array.mk with wrong column or row count in an effect. PDDL-XTS: X_whole_array_size_mismatch_2d"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_size_mismatch_2d')
    val_t = IntType(0, 9)

    grid = Fluent('grid', ArrayType(2, ArrayType(3, val_t)))
    p.add_fluent(grid)
    p.set_initial_value(grid, [[0, 0, 0], [0, 0, 0]])

    # Error: inner arrays have 2 elements instead of 3 (wrong column count)
    load_wrong_cols = InstantaneousAction('load_wrong_cols')
    load_wrong_cols.add_effect(grid, [[1, 2], [3, 4]])
    p.add_action(load_wrong_cols)

    # Error: outer array has 3 rows instead of 2
    load_wrong_rows = InstantaneousAction('load_wrong_rows')
    load_wrong_rows.add_effect(grid, [[1, 2, 3], [4, 5, 6], [7, 8, 9]])
    p.add_action(load_wrong_rows)

    p.add_goal(Equals(grid[0][0], 1))
    return p
