"""
2D SV-to-SV copy — (assign (dst) (src)) for a 2×3 array.
copy_all: dst := src in one effect.
Initial: src=[[1,2,3],[4,5,6]], dst=zeros; goal: dst[1][2]=6.
Plan: copy_all()  →  dst=[[1,2,3],[4,5,6]].
PDDL-XTS: whole_2d_sv_assign
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('sv_assign_2d')
    val_t = IntType(0, 9)

    src = Fluent('src', ArrayType(2, ArrayType(3, val_t)))
    dst = Fluent('dst', ArrayType(2, ArrayType(3, val_t)))
    p.add_fluent(src)
    p.add_fluent(dst)

    p.set_initial_value(src, [[1, 2, 3], [4, 5, 6]])
    p.set_initial_value(dst, [[0, 0, 0], [0, 0, 0]])

    copy_all = InstantaneousAction('copy_all')
    copy_all.add_precondition(Equals(dst[0][0], 0))
    copy_all.add_effect(dst, src)
    p.add_action(copy_all)

    p.add_goal(Equals(dst[1][2], 6))
    return p
