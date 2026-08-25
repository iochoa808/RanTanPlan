"""SetAdd with constant above the element type's upper bound. PDDL-XTS: X_sem_const_set_oob"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_sem_const_set_oob')

    val_t  = IntType(0, 9)
    valset = SetType(val_t)

    chain = Fluent('chain', valset)
    p.add_fluent(chain, default_initial_value=set())

    # Error: Int(10) > ub=9, out of range for SetType(IntType(0,9))
    overflow = InstantaneousAction('overflow')
    overflow.add_effect(chain, SetAdd(Int(10), chain))
    p.add_action(overflow)

    # Goal references the OOB element — unreachable even if action fires
    p.add_goal(SetMember(Int(10), chain))
    return p