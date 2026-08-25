"""Assigning a Python set of action Parameter FNodes as a whole-set effect. PDDL-XTS: X_set_mk_params_effect"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_set_mk_params_effect')

    Item = UserType('Item')

    item_a = Object('item_a', Item)
    item_b = Object('item_b', Item)
    item_c = Object('item_c', Item)
    item_d = Object('item_d', Item)
    p.add_objects([item_a, item_b, item_c, item_d])

    basket = Fluent('basket', SetType(Item))
    p.add_fluent(basket, default_initial_value=set())
    p.set_initial_value(basket, {item_a, item_b, item_c})

    # The Python API accepts {a, b} (Parameter FNodes) as a valid set assignment
    # (unlike PDDL which rejects parameter references inside set.mk literals).
    reset_pair = InstantaneousAction('reset_pair', a=Item, b=Item)
    a, b = reset_pair.parameter('a'), reset_pair.parameter('b')
    reset_pair.add_precondition(SetMember(a, basket))
    reset_pair.add_precondition(SetMember(b, basket))
    reset_pair.add_effect(basket, {a, b})
    p.add_action(reset_pair)

    p.add_goal(Equals(basket, {item_a, item_b}))
    return p