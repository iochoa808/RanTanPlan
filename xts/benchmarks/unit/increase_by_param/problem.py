"""
Step counter — increase/decrease with bounded-int parameter as step amount.
boost(step): counter += step.  cut(step): counter -= step.
Initial counter=1; goal: counter=7.
Plan: boost(3), boost(3)  →  1+3+3=7.
PDDL-XTS: increase_by_param
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('step_counter')
    count_t = IntType(0, 10)
    step_t  = IntType(1, 4)

    counter = Fluent('counter', count_t)
    p.add_fluent(counter, default_initial_value=0)
    p.set_initial_value(counter, 1)

    boost = InstantaneousAction('boost', step=step_t)
    step = boost.parameter('step')
    boost.add_precondition(LE(Plus(counter, step), 10))
    boost.add_effect(counter, Plus(counter, step))
    p.add_action(boost)

    cut = InstantaneousAction('cut', step=step_t)
    step2 = cut.parameter('step')
    cut.add_precondition(GE(Minus(counter, step2), 0))
    cut.add_effect(counter, Minus(counter, step2))
    p.add_action(cut)

    p.add_goal(Equals(counter, 7))
    return p
