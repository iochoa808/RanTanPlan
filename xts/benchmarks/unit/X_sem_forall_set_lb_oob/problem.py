"""Forall effect adds set element below lower bound (i-1 where i starts at lb). PDDL-XTS: X_sem_forall_set_lb_oob"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_sem_forall_set_lb_oob')

    slot_t  = IntType(2, 9)   # lower bound is 2
    low_t   = IntType(2, 3)
    slotset = SetType(slot_t)

    zone = Fluent('zone', slotset)
    p.add_fluent(zone, default_initial_value=set())
    p.set_initial_value(zone, {Int(2), Int(3)})

    # Error: when i=2, Minus(rv, Int(1)) = 1 which is < lb=2 — out of range
    bulk_drop = InstantaneousAction('bulk_drop')
    rv = RangeVariable('i', 2, 3)
    bulk_drop.add_effect(zone, SetAdd(Minus(rv, Int(1)), zone), forall=[rv])
    p.add_action(bulk_drop)

    # Goal is unreachable
    p.add_goal(SetMember(Int(0), zone))
    return p