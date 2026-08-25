"""Arithmetic addition that always exceeds the type's upper bound. PDDL-XTS: X_sem_increase_overflow"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_sem_increase_overflow')

    count_t = IntType(0, 5)

    counter = Fluent('counter', count_t)
    p.add_fluent(counter, default_initial_value=0)

    # Error: 0 + 100 = 100 which far exceeds ub=5
    blast = InstantaneousAction('blast')
    blast.add_effect(counter, Plus(counter, Int(100)))
    p.add_action(blast)

    # Safe: only fires when counter <= 2, adds 3 (max result = 5 = ub)
    safe_increase = InstantaneousAction('safe_increase')
    safe_increase.add_precondition(LE(counter, Int(2)))
    safe_increase.add_effect(counter, Plus(counter, Int(3)))
    p.add_action(safe_increase)

    p.add_goal(Equals(counter, 5))
    return p