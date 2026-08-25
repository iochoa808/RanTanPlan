"""
Disjoint sets — SetDisjoint in precondition gates an action.
PDDL-XTS: set_disjoint
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('set_disjoint')

    Item = UserType('Item')
    item_a = Object('item_a', Item)
    item_b = Object('item_b', Item)
    item_c = Object('item_c', Item)
    item_d = Object('item_d', Item)
    p.add_objects([item_a, item_b, item_c, item_d])

    flag_t   = IntType(0, 1)
    owned      = Fluent('owned',      SetType(Item))
    restricted = Fluent('restricted', SetType(Item))
    cleared    = Fluent('cleared',    flag_t)
    p.add_fluent(owned,      default_initial_value=set())
    p.add_fluent(restricted, default_initial_value=set())
    p.add_fluent(cleared,    default_initial_value=0)

    p.set_initial_value(owned,      {item_a, item_b})
    p.set_initial_value(restricted, {item_c, item_d})
    p.set_initial_value(cleared,    0)

    proceed = InstantaneousAction('proceed')
    proceed.add_precondition(Equals(cleared, 0))
    proceed.add_precondition(SetDisjoint(owned, restricted))
    proceed.add_effect(cleared, 1)
    p.add_action(proceed)

    p.add_goal(Equals(cleared, 1))
    return p
