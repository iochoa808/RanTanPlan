"""
Whole-array SV-to-SV assign: copy_all assigns the entire src array into dst.
PDDL-XTS: whole_sv_assign
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('sv_assign')

    val_t = IntType(0, 9)

    src = Fluent('src', ArrayType(3, val_t))
    dst = Fluent('dst', ArrayType(3, val_t))
    p.add_fluent(src)
    p.add_fluent(dst)

    p.set_initial_value(src, [4, 5, 6])
    p.set_initial_value(dst, [0, 0, 0])

    copy_all = InstantaneousAction('copy_all')
    copy_all.add_effect(dst, src)
    p.add_action(copy_all)

    p.add_goal(Equals(dst[Int(0)], Int(4)))
    p.add_goal(Equals(dst[Int(2)], Int(6)))
    return p