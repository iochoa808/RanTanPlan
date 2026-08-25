"""
Nested arithmetic — multi-level expressions: (a*b)+c and a*(b+1).
compute1: result = (a*b)+c = 10.
Initial: a=2, b=3, c=4, result=0; goal: result=10.
Plan: compute1()  →  result=10.
PDDL-XTS: nested_arithmetic
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('nested_arith')
    small_t  = IntType(0, 5)
    result_t = IntType(0, 30)

    a      = Fluent('a',      small_t)
    b      = Fluent('b',      small_t)
    c      = Fluent('c',      small_t)
    result = Fluent('result', result_t)
    p.add_fluent(a,      default_initial_value=0)
    p.add_fluent(b,      default_initial_value=0)
    p.add_fluent(c,      default_initial_value=0)
    p.add_fluent(result, default_initial_value=0)
    p.set_initial_value(a, 2)
    p.set_initial_value(b, 3)
    p.set_initial_value(c, 4)
    p.set_initial_value(result, 0)

    # result = (a * b) + c
    compute1 = InstantaneousAction('compute1')
    compute1.add_precondition(Equals(result, 0))
    compute1.add_effect(result, Plus(Times(a, b), c))
    p.add_action(compute1)

    # result = a * (b + 1)
    compute2 = InstantaneousAction('compute2')
    compute2.add_precondition(Equals(result, 0))
    compute2.add_effect(result, Times(a, Plus(b, 1)))
    p.add_action(compute2)

    p.add_goal(Equals(result, 10))
    return p
