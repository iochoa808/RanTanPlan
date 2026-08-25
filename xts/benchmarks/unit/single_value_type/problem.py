"""
Single-value type — (number 2 2): the only valid parameter value is 2.
boost(s): counter += s; goal=4 forces exactly two boost(2) steps.
Initial: counter=0; goal: counter=4.
Plan: boost(2), boost(2).
PDDL-XTS: single_value_type
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('single_value')
    count_t = IntType(0, 10)
    step_t  = IntType(2, 2)   # single-value type: only 2 is valid

    counter = Fluent('counter', count_t)
    p.add_fluent(counter, default_initial_value=0)
    p.set_initial_value(counter, 0)

    boost = InstantaneousAction('boost', s=step_t)
    s = boost.parameter('s')
    boost.add_precondition(LE(Plus(counter, s), 10))
    boost.add_effect(counter, Plus(counter, s))
    p.add_action(boost)

    p.add_goal(Equals(counter, 4))
    return p
