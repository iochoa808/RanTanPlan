"""SetRemove with constant element exceeding upper bound of element type. PDDL-XTS: X_sem_set_remove_oob"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_sem_set_remove_oob')

    val_t  = IntType(0, 9)
    valset = SetType(val_t)

    chain = Fluent('chain', valset)
    p.add_fluent(chain, default_initial_value=set())
    p.set_initial_value(chain, {Int(3), Int(7)})

    # Error: Int(10) > ub=9, out of range for SetType(IntType(0,9))
    bad_remove = InstantaneousAction('bad_remove')
    bad_remove.add_effect(chain, SetRemove(Int(10), chain))
    p.add_action(bad_remove)

    # Goal is unreachable: 10 is never a valid member
    p.add_goal(SetMember(Int(10), chain))
    return p