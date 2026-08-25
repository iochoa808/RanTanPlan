"""Bounded integer with negative lower bound (-5..5). PDDL-XTS: neg_bounded_bounds"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('neg_bounded_bounds')

    temp_t = IntType(-5, 5)
    t = Fluent('t', temp_t)
    p.add_fluent(t, default_initial_value=-3)

    warm = InstantaneousAction('warm')
    warm.add_precondition(LT(t, Int(5)))
    warm.add_effect(t, Plus(t, Int(1)))
    p.add_action(warm)

    p.set_initial_value(t, -3)

    p.add_goal(Equals(t, Int(2)))
    return p