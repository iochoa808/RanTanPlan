"""
Dual 2D — copy 2×2 src grid into dst grid, cell by cell.
PDDL-XTS: multi_2d_array
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('dual_2d')

    row_t = IntType(0, 1)
    col_t = IntType(0, 1)
    val_t = IntType(0, 4)
    grid_t = ArrayType(2, ArrayType(2, val_t))

    src = Fluent('src', grid_t)
    dst = Fluent('dst', grid_t)
    p.add_fluent(src)
    p.add_fluent(dst)
    p.set_initial_value(src, [[3, 1], [4, 2]])
    p.set_initial_value(dst, [[0, 0], [0, 0]])

    copy = InstantaneousAction('copy', r=row_t, c=col_t, v=val_t)
    r, c, v = [copy.parameter(x) for x in ('r','c','v')]
    copy.add_precondition(Equals(src[r][c], v))
    copy.add_precondition(Not(Equals(dst[r][c], v)))
    copy.add_effect(dst[r][c], v)
    p.add_action(copy)

    p.add_goal(Equals(dst, [[3, 1], [4, 2]]))
    return p
