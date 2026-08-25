"""
Dual 3D — copy 2×2×2 src tensor into dst tensor, cell by cell.
PDDL-XTS: multi_3d_array
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('dual_3d')

    idx_t = IntType(0, 1)
    val_t = IntType(0, 4)
    cube_t = ArrayType(2, ArrayType(2, ArrayType(2, val_t)))

    src = Fluent('src', cube_t)
    dst = Fluent('dst', cube_t)
    p.add_fluent(src)
    p.add_fluent(dst)
    p.set_initial_value(src, [[[1,2],[3,4]],[[0,1],[2,3]]])
    p.set_initial_value(dst, [[[0,0],[0,0]],[[0,0],[0,0]]])

    copy = InstantaneousAction('copy', d=idx_t, r=idx_t, c=idx_t, v=val_t)
    d, r, c, v = [copy.parameter(x) for x in ('d','r','c','v')]
    copy.add_precondition(Equals(src[d][r][c], v))
    copy.add_precondition(Not(Equals(dst[d][r][c], v)))
    copy.add_effect(dst[d][r][c], v)
    p.add_action(copy)

    p.add_goal(Equals(dst, [[[1,2],[3,4]],[[0,1],[2,3]]]))
    return p
