"""
Basket reset — assign set.mk literal in action effect.
PDDL-XTS: setmk_effect
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('reset_basket')

    Item = UserType('Item')
    item_a = Object('item_a', Item)
    item_b = Object('item_b', Item)
    item_c = Object('item_c', Item)
    item_d = Object('item_d', Item)
    p.add_objects([item_a, item_b, item_c, item_d])

    basket = Fluent('basket', SetType(Item))
    p.add_fluent(basket, default_initial_value=set())
    p.set_initial_value(basket, {item_c, item_d, item_a})

    add_item = InstantaneousAction('add_item', x=Item)
    x = add_item.parameter('x')
    add_item.add_precondition(Not(SetMember(x, basket)))
    add_item.add_effect(basket, SetAdd(x, basket))
    p.add_action(add_item)

    reset_to_ab = InstantaneousAction('reset_to_ab')
    reset_to_ab.add_precondition(GE(SetCardinality(basket), 3))
    reset_to_ab.add_effect(basket, {item_a, item_b})
    p.add_action(reset_to_ab)

    p.add_goal(SetMember(item_a, basket))
    p.add_goal(SetMember(item_b, basket))
    p.add_goal(Not(SetMember(item_c, basket)))
    return p
