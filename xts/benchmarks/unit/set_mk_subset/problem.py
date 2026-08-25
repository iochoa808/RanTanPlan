"""
Approval — subset and disjoint against inline set.mk literals (PDDL) /
static reference fluents (Python).
approve: basket ⊆ allowed={0,2,4} AND odds ∩ forbidden={1,3} = ∅  →  approved=1.
Initial: basket={0,2}, odds={}, allowed={0,2,4}, forbidden={1,3}; goal: approved=1.
Plan: approve()  →  1 step.
PDDL-XTS: set_mk_subset
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('approval')
    val_t  = IntType(0, 4)
    flag_t = IntType(0, 1)

    basket   = Fluent('basket',   SetType(val_t))
    odds     = Fluent('odds',     SetType(val_t))
    allowed  = Fluent('allowed',  SetType(val_t))   # static: {0,2,4}
    forb     = Fluent('forb',     SetType(val_t))   # static: {1,3}
    approved = Fluent('approved', flag_t)
    p.add_fluent(basket,   default_initial_value=set())
    p.add_fluent(odds,     default_initial_value=set())
    p.add_fluent(allowed,  default_initial_value=set())
    p.add_fluent(forb,     default_initial_value=set())
    p.add_fluent(approved, default_initial_value=0)

    p.set_initial_value(basket,   {Int(0), Int(2)})
    p.set_initial_value(odds,     set())
    p.set_initial_value(allowed,  {Int(0), Int(2), Int(4)})
    p.set_initial_value(forb,     {Int(1), Int(3)})
    p.set_initial_value(approved, 0)

    approve = InstantaneousAction('approve')
    approve.add_precondition(Equals(approved, 0))
    approve.add_precondition(SetSubseteq(basket, allowed))
    approve.add_precondition(SetDisjoint(odds, forb))
    approve.add_effect(approved, 1)
    p.add_action(approve)

    p.add_goal(Equals(approved, 1))
    return p
