"""Arithmetic on set element produces value exceeding element type bounds. PDDL-XTS: X_sem_arith_elem_exceeds_type"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_sem_arith_elem_exceeds_type')

    val_t = IntType(0, 9)
    top_t = IntType(8, 9)

    chain = Fluent('chain', SetType(val_t))
    p.add_fluent(chain, default_initial_value=set())
    p.set_initial_value(chain, {Int(8), Int(9)})

    # Error: when n=9, Plus(n, Int(1)) = 10 which exceeds upper bound 9 of val_t
    step_overflow = InstantaneousAction('step_overflow', n=top_t)
    n = step_overflow.parameter('n')
    step_overflow.add_precondition(SetMember(n, chain))
    step_overflow.add_effect(chain, SetAdd(Plus(n, Int(1)), chain))
    p.add_action(step_overflow)

    # Goal is unreachable (7 < 8 = lower bound of top_t, so step_overflow can never produce 7)
    p.add_goal(SetMember(Int(7), chain))
    return p