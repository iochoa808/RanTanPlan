"""SetAdd with constant below the element type's lower bound. PDDL-XTS: X_sem_const_set_lb_oob"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_sem_const_set_lb_oob')

    slot_t  = IntType(5, 9)   # lower bound is 5
    slotset = SetType(slot_t)

    restricted = Fluent('restricted', slotset)
    p.add_fluent(restricted, default_initial_value=set())

    # Error: Int(4) < lb=5, so adding it to a SetType(IntType(5,9)) is out of range
    underflow = InstantaneousAction('underflow')
    underflow.add_effect(restricted, SetAdd(Int(4), restricted))
    p.add_action(underflow)

    # Goal is unreachable: 4 < lb=5 so it can never be a valid member
    p.add_goal(SetMember(Int(4), restricted))
    return p