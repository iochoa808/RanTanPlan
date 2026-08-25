"""Initial values outside bounded integer type range for parameterised fluent. PDDL-XTS: X_bounded_init_overflow"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_bounded_init_overflow')

    Room        = UserType('Room')
    temperature = Fluent('temperature', IntType(0, 5), r=Room)
    p.add_fluent(temperature, default_initial_value=0)

    room1 = Object('room1', Room)
    room2 = Object('room2', Room)
    p.add_objects([room1, room2])

    # Error: 99 > 5 and -3 < 0 — both out of range for IntType(0,5)
    p.set_initial_value(temperature(room1), 99)
    p.set_initial_value(temperature(room2), -3)

    p.add_goal(Equals(temperature(room1), 99))
    return p