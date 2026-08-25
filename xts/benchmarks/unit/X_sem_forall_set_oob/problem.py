"""Forall effect adds set element above upper bound (i+1 where i reaches ub). PDDL-XTS: X_sem_forall_set_oob"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_sem_forall_set_oob')

    val_t  = IntType(0, 9)
    top_t  = IntType(8, 9)
    valset = SetType(val_t)

    chain = Fluent('chain', valset)
    p.add_fluent(chain, default_initial_value=set())
    p.set_initial_value(chain, {Int(8), Int(9)})

    # Error: when i=9, Plus(rv, Int(1)) = 10 which exceeds ub=9
    bulk_step = InstantaneousAction('bulk_step')
    rv = RangeVariable('i', 8, 9)
    bulk_step.add_effect(chain, SetAdd(Plus(rv, Int(1)), chain), forall=[rv])
    p.add_action(bulk_step)

    # Goal is unreachable: 7 is never produced by the action
    p.add_goal(SetMember(Int(7), chain))
    return p