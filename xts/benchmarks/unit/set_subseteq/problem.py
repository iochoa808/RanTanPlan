"""
Subset check — SetSubseteq in precondition gates shipping.
PDDL-XTS: set_subseteq
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('set_subseteq')

    item_t  = IntType(0, 4)
    flag_t  = IntType(0, 1)
    needed  = Fluent('needed',  SetType(item_t))
    stocked = Fluent('stocked', SetType(item_t))
    shipped = Fluent('shipped', flag_t)
    p.add_fluent(needed,  default_initial_value=set())
    p.add_fluent(stocked, default_initial_value=set())
    p.add_fluent(shipped, default_initial_value=0)

    p.set_initial_value(needed,  {Int(1), Int(2)})
    p.set_initial_value(stocked, {Int(1), Int(2), Int(3)})
    p.set_initial_value(shipped, 0)

    ship = InstantaneousAction('ship')
    ship.add_precondition(Equals(shipped, 0))
    ship.add_precondition(SetSubseteq(needed, stocked))
    ship.add_effect(shipped, 1)
    p.add_action(ship)

    p.add_goal(Equals(shipped, 1))
    return p
