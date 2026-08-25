"""
Thermostat — rooms with bounded-int temperature, heat/cool to target.
PDDL-XTS: bounded
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('thermostat')
    Room = UserType('Room')
    kitchen = Object('kitchen', Room)
    living_room = Object('living_room', Room)
    p.add_objects([kitchen, living_room])

    current_temp = Fluent('current_temp', IntType(15, 25), r=Room)
    target_temp  = Fluent('target_temp',  IntType(18, 23), r=Room)
    p.add_fluent(current_temp)
    p.add_fluent(target_temp)

    p.set_initial_value(current_temp(kitchen),     16)
    p.set_initial_value(target_temp(kitchen),      20)
    p.set_initial_value(current_temp(living_room), 25)
    p.set_initial_value(target_temp(living_room),  22)

    heat_up = InstantaneousAction('heat_up', r=Room)
    r = heat_up.parameter('r')
    heat_up.add_precondition(LT(current_temp(r), target_temp(r)))
    heat_up.add_effect(current_temp(r), Plus(current_temp(r), 1))
    p.add_action(heat_up)

    cool_down = InstantaneousAction('cool_down', r=Room)
    r = cool_down.parameter('r')
    cool_down.add_precondition(GT(current_temp(r), target_temp(r)))
    cool_down.add_effect(current_temp(r), Minus(current_temp(r), 1))
    p.add_action(cool_down)

    p.add_goal(Equals(current_temp(kitchen),     20))
    p.add_goal(Equals(current_temp(living_room), 22))
    return p
