"""
Two-element arrays; copy src into dst in one action.
Default (satisficing): SOLVED 1 step.  Optimal mode: false UNSOLVABLE_PROVEN (known bug).
PDDL-XTS: bnb_optimal_unsound
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('bnb_optimal_unsound')

    val_t = IntType(0, 9)

    src = Fluent('src', ArrayType(2, val_t))
    dst = Fluent('dst', ArrayType(2, val_t))
    p.add_fluent(src)
    p.add_fluent(dst)

    p.set_initial_value(src, [3, 7])
    p.set_initial_value(dst, [0, 0])

    copy = InstantaneousAction('copy')
    copy.add_effect(dst, src)
    p.add_action(copy)

    p.add_goal(Equals(dst[Int(1)], Int(7)))
    return p