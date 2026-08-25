"""Whole-array assignment in effect with wrong element count. PDDL-XTS: X_effect_array_mk_overcount"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_effect_array_mk_overcount')

    val_t = IntType(0, 9)

    a = Fluent('a', ArrayType(3, val_t))
    p.add_fluent(a)
    p.set_initial_value(a, [0, 0, 0])

    # Error: assigning 5 elements to a size-3 array
    bad = InstantaneousAction('bad')
    bad.add_effect(a, [1, 2, 3, 4, 5])
    p.add_action(bad)

    p.add_goal(Equals(a[Int(0)], 1))
    return p