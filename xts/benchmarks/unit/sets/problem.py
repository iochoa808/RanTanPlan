"""
Basket — basic set operations: pick_up, put_down, dump.
PDDL-XTS: sets
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('sets_test')

    Item = UserType('Item')
    apple  = Object('apple',  Item)
    banana = Object('banana', Item)
    cherry = Object('cherry', Item)
    orange = Object('orange', Item)
    p.add_objects([apple, banana, cherry, orange])

    basket = Fluent('basket', SetType(Item))
    p.add_fluent(basket, default_initial_value=set())
    p.set_initial_value(basket, {cherry, orange})

    pick_up = InstantaneousAction('pick_up', x=Item)
    x = pick_up.parameter('x')
    pick_up.add_precondition(Not(SetMember(x, basket)))
    pick_up.add_effect(basket, SetAdd(x, basket))
    p.add_action(pick_up)

    put_down = InstantaneousAction('put_down', x=Item)
    x = put_down.parameter('x')
    put_down.add_precondition(SetMember(x, basket))
    put_down.add_effect(basket, SetRemove(x, basket))
    p.add_action(put_down)

    dump_basket = InstantaneousAction('dump_basket')
    dump_basket.add_effect(basket, SetDifference(basket, basket))
    p.add_action(dump_basket)

    p.add_goal(Equals(basket, {apple, banana}))
    return p
