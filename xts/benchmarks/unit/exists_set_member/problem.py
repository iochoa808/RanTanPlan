"""
Sensor — exists over integer range with set membership in body.
detect fires when any integer in [3..7] is in the bag (exists ?i in (3..7): ?i ∈ bag).
Initial: bag={}.  Goal: detected=True.
Plan: fill (adds 7 to bag), detect (7 ∈ bag satisfies exists).
PDDL-XTS: exists_set_member
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('sensor')

    val_t = IntType(0, 9)

    bag      = Fluent('bag',      SetType(val_t))
    detected = Fluent('detected')
    p.add_fluent(bag,      default_initial_value=set())
    p.add_fluent(detected, default_initial_value=False)

    p.set_initial_value(bag,      set())
    p.set_initial_value(detected, False)

    fill = InstantaneousAction('fill')
    fill.add_precondition(Not(SetMember(Int(7), bag)))
    fill.add_effect(bag, SetAdd(Int(7), bag))
    p.add_action(fill)

    # detect: exists ?i in (3..7): member(?i, bag)
    rv = RangeVariable('i', 3, 7)
    detect = InstantaneousAction('detect')
    detect.add_precondition(Not(detected))
    detect.add_precondition(Exists(SetMember(rv, bag), rv))
    detect.add_effect(detected, True)
    p.add_action(detect)

    p.add_goal(detected)
    return p