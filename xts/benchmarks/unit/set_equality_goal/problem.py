"""
Basket — goal asserts whole-set literal equality: basket = {item_a, item_b}.
add(x): add item x to basket.
Initial: basket={}; goal: basket={item_a, item_b}.
Plan: add(item_a), add(item_b)  →  2 steps.
PDDL-XTS: set_equality_goal
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('basket')

    Item   = UserType('Item')
    item_a = Object('item_a', Item)
    item_b = Object('item_b', Item)
    item_c = Object('item_c', Item)
    p.add_objects([item_a, item_b, item_c])

    basket = Fluent('basket', SetType(Item))
    p.add_fluent(basket, default_initial_value=set())
    p.set_initial_value(basket, set())

    add = InstantaneousAction('add', x=Item)
    x = add.parameter('x')
    add.add_precondition(Not(SetMember(x, basket)))
    add.add_effect(basket, SetAdd(x, basket))
    p.add_action(add)

    p.add_goal(Equals(basket, {item_a, item_b}))
    return p
