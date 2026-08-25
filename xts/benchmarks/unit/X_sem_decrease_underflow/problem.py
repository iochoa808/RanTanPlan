"""Arithmetic subtraction that always produces values below the type's lower bound. PDDL-XTS: X_sem_decrease_underflow"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_sem_decrease_underflow')

    level_t  = IntType(3, 9)   # lower bound is 3

    altitude = Fluent('altitude', level_t)
    p.add_fluent(altitude, default_initial_value=9)

    # Error: altitude - 50 is always negative (well below lb=3) for any altitude in [3,9]
    blast_down = InstantaneousAction('blast_down')
    blast_down.add_effect(altitude, Minus(altitude, Int(50)))
    p.add_action(blast_down)

    # Error: altitude - 7 goes below lb=3 for all valid altitudes in [3,9] (only 9 would give 2)
    drain = InstantaneousAction('drain')
    drain.add_effect(altitude, Minus(altitude, Int(7)))
    p.add_action(drain)

    # Safe: only fires when altitude > 3, decrements by 1 (stays in [3,9])
    descend = InstantaneousAction('descend')
    descend.add_precondition(GT(altitude, Int(3)))
    descend.add_effect(altitude, Minus(altitude, Int(1)))
    p.add_action(descend)

    p.add_goal(Equals(altitude, 3))
    return p