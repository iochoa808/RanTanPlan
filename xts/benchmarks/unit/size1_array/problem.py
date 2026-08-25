"""
Degenerate size-1 array: bump cell 0 from 0 to 5.
PDDL-XTS: size1_array
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('size1')

    val_t = IntType(0, 9)

    a = Fluent('a', ArrayType(1, val_t))
    p.add_fluent(a)
    p.set_initial_value(a, [0])

    bump = InstantaneousAction('bump')
    bump.add_precondition(Equals(a[Int(0)], Int(0)))
    bump.add_effect(a[Int(0)], Int(5))
    p.add_action(bump)

    p.add_goal(Equals(a[Int(0)], Int(5)))
    return p