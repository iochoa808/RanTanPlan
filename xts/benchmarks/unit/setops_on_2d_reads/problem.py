"""
Grid inspector — set cardinality on 2D array of int-sets.
PDDL-XTS: setops_on_2d_reads
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('grid_inspector')

    row_t   = IntType(0, 1)
    col_t   = IntType(0, 2)
    val_t   = IntType(0, 4)
    count_t = IntType(0, 6)

    cells = Fluent('cells', ArrayType(2, ArrayType(3, SetType(val_t))))
    big   = Fluent('big',   count_t)
    p.add_fluent(cells)
    p.add_fluent(big, default_initial_value=0)

    p.set_initial_value(cells, [
        [{Int(0), Int(1)}, {Int(2)},              {Int(3), Int(4)}],
        [{Int(1), Int(2)}, {Int(0), Int(2), Int(4)}, {Int(3)}],
    ])
    p.set_initial_value(big, 0)

    count_big = InstantaneousAction('count_big', r=row_t, c=col_t)
    r, c = count_big.parameter('r'), count_big.parameter('c')
    count_big.add_precondition(LT(big, 6))
    count_big.add_precondition(GE(SetCardinality(cells[r][c]), 2))
    count_big.add_effect(big, Plus(big, 1))
    p.add_action(count_big)

    p.add_goal(Equals(big, 4))
    return p
