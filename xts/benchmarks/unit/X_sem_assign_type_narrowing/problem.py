"""Assigning a wider-typed fluent value to a narrower-typed fluent. PDDL-XTS: X_sem_assign_type_narrowing"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_sem_assign_type_narrowing')

    temp_t     = IntType(15, 25)
    setpoint_t = IntType(18, 23)
    Room       = UserType('Room')

    current = Fluent('current', temp_t,     r=Room)
    target  = Fluent('target',  setpoint_t, r=Room)
    p.add_fluent(current, default_initial_value=20)
    p.add_fluent(target,  default_initial_value=20)

    room1 = Object('room1', Room)
    p.add_object(room1)

    p.set_initial_value(current(room1), 15)
    p.set_initial_value(target(room1),  20)

    # Error: current has range [15,25] which is wider than target's [18,23]
    # Assigning current(r) → target(r) may produce values outside target's type
    sync_temp = InstantaneousAction('sync_temp', r=Room)
    r = sync_temp.parameter('r')
    sync_temp.add_effect(target(r), current(r))
    p.add_action(sync_temp)

    p.add_goal(Equals(target(room1), 15))
    return p