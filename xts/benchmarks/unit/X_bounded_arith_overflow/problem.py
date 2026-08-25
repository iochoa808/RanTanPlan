"""Arithmetic results exceeding bounded integer type limits. PDDL-XTS: X_bounded_arith_overflow"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_bounded_arith_overflow')

    bound_t = IntType(0, 5)

    a = Fluent('a', bound_t)
    b = Fluent('b', bound_t)
    c = Fluent('c', bound_t)
    p.add_fluent(a)
    p.add_fluent(b)
    p.add_fluent(c)
    p.set_initial_value(a, 4)
    p.set_initial_value(b, 5)
    p.set_initial_value(c, 0)

    # Error: a+b can reach 9 which exceeds bound of 5
    combine = InstantaneousAction('combine')
    combine.add_effect(c, Plus(a, b))
    p.add_action(combine)

    # Error: a*b can reach 25 which exceeds bound of 5
    multiply = InstantaneousAction('multiply')
    multiply.add_effect(c, Times(a, b))
    p.add_action(multiply)

    # Error: 0-a can produce negative values, below lower bound 0
    negate = InstantaneousAction('negate')
    negate.add_effect(c, Minus(Int(0), a))
    p.add_action(negate)

    p.add_goal(Equals(c, 9))
    return p