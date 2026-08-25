"""
Forall set in bounds — single-iteration forall add keeps element within type range.
PDDL-XTS: sem_forall_set_in_bounds
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('sem_forall_set_in_bounds')

    val_t = IntType(0, 9)
    chain = Fluent('chain', SetType(val_t))
    p.add_fluent(chain, default_initial_value=set())
    p.set_initial_value(chain, {Int(0)})

    add_top = InstantaneousAction('add_top')
    add_top.add_precondition(Not(SetMember(Int(9), chain)))
    rv = RangeVariable('i', 8, 8)
    add_top.add_effect(chain, SetAdd(Plus(rv, 1), chain), forall=[rv])
    p.add_action(add_top)

    p.add_goal(SetMember(Int(9), chain))
    return p
