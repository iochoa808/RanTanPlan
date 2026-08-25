"""
Counter — set_counter (assign from param) and inc (assign counter + param).
set_counter is limited to v ≤ 4; goal = 5 forces subsequent use of inc.
Initial counter = 0; goal: counter = 5.
Plan: set_counter(4) → counter=4, inc(1) → counter=5.
PDDL-XTS: bounded_params
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('counter')
    counter = Fluent('counter', IntType(0, 5))
    p.add_fluent(counter, default_initial_value=0)
    p.set_initial_value(counter, 0)

    # assign counter = v  (param-value direct assignment)
    set_counter = InstantaneousAction('set_counter', v=IntType(0, 4))
    v = set_counter.parameter('v')
    set_counter.add_precondition(LT(counter, v))
    set_counter.add_effect(counter, v)
    p.add_action(set_counter)

    # assign counter = counter + v  (param-value arithmetic assignment)
    inc = InstantaneousAction('inc', v=IntType(1, 5))
    v2 = inc.parameter('v')
    inc.add_precondition(LE(Plus(counter, v2), 5))
    inc.add_effect(counter, Plus(counter, v2))
    p.add_action(inc)

    p.add_goal(Equals(counter, 5))
    return p
