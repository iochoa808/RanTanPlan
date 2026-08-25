"""Arithmetic element in remove exceeds the set's element type range. PDDL-XTS: X_sem_remove_arithmetic_exceeds_type"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_remove_arith_exceeds')

    # parameter type [7,9]; element type [0,9]
    # remove(n+3): when n=7 → removes element 10, which exceeds [0,9]
    param_t = IntType(7, 9)
    elem_t  = IntType(0, 9)

    bag  = Fluent('bag',  SetType(elem_t))
    done = Fluent('done', BoolType())
    p.add_fluent(bag,  default_initial_value=set())
    p.add_fluent(done, default_initial_value=False)
    p.set_initial_value(bag, {Int(7), Int(8), Int(9)})

    # Error: remove (n+3) where n ∈ [7,9] → element can be 10, 11, 12 (all > 9)
    bad_remove = InstantaneousAction('bad_remove', n=param_t)
    n = bad_remove.parameter('n')
    bad_remove.add_precondition(SetMember(n, bag))
    bad_remove.add_effect(bag, SetRemove(Plus(n, Int(3)), bag))
    bad_remove.add_effect(done, True)
    p.add_action(bad_remove)

    p.add_goal(done)
    return p
