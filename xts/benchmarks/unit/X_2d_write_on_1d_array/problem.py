"""Indexing scalar result of 1D read and assigning scalar to sub-array row. PDDL-XTS: X_2d_write_on_1d_array"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_2d_write_on_1d_array')

    row_t = IntType(0, 2)
    col_t = IntType(0, 2)
    val_t = IntType(0, 9)

    row  = Fluent('row',  ArrayType(3, val_t))
    grid = Fluent('grid', ArrayType(3, ArrayType(3, val_t)))
    p.add_fluent(row)
    p.add_fluent(grid)
    p.set_initial_value(row,  [0, 0, 0])
    p.set_initial_value(grid, [[0,0,0],[0,0,0],[0,0,0]])

    # Error: row[i] is a scalar (IntType), indexing it again → type error
    fill_2d = InstantaneousAction('fill_2d', i=row_t, j=col_t)
    i, j = fill_2d.parameter('i'), fill_2d.parameter('j')
    fill_2d.add_effect(row[i][j], 7)
    p.add_action(fill_2d)

    # Error: assigning scalar 5 to a sub-array (ArrayType row) → type error
    fill_1d = InstantaneousAction('fill_1d', i=row_t)
    i2 = fill_1d.parameter('i')
    fill_1d.add_effect(grid[i2], 5)
    p.add_action(fill_1d)

    p.add_goal(Equals(row[Int(0)], 7))
    return p