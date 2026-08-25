"""
Dual array — copy src array into dst array, slot by slot.
PDDL-XTS: multi_array
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('dual_array')
    val_t = IntType(0, 4)

    src = Fluent('src', ArrayType(3, val_t))
    dst = Fluent('dst', ArrayType(3, val_t))
    p.add_fluent(src)
    p.add_fluent(dst)
    p.set_initial_value(src, [3, 1, 4])
    p.set_initial_value(dst, [0, 0, 0])

    for idx in range(3):
        act = InstantaneousAction(f'copy_{idx}', v=val_t)
        v = act.parameter('v')
        act.add_precondition(Equals(src[idx], v))
        act.add_precondition(Not(Equals(dst[idx], v)))
        act.add_effect(dst[idx], v)
        p.add_action(act)

    p.add_goal(Equals(dst, [3, 1, 4]))
    return p
