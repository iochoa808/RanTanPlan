"""SetDifference applied to sets of mismatched element types. PDDL-XTS: X_difference_type_mismatch"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_difference_type_mismatch')

    Item    = UserType('Item')
    level_t = IntType(0, 5)

    item_a = Object('item_a', Item)
    item_b = Object('item_b', Item)
    p.add_objects([item_a, item_b])

    obj_bag = Fluent('obj_bag', SetType(Item))
    int_bag = Fluent('int_bag', SetType(level_t))
    result  = Fluent('result',  SetType(Item))
    done    = Fluent('done',    BoolType())
    p.add_fluent(obj_bag, default_initial_value=set())
    p.add_fluent(int_bag, default_initial_value=set())
    p.add_fluent(result,  default_initial_value=set())
    p.add_fluent(done,    default_initial_value=False)
    p.set_initial_value(obj_bag, {item_a, item_b})
    p.set_initial_value(int_bag, {Int(1), Int(3)})

    # Error: SetDifference requires both operands to have the same element type
    bad = InstantaneousAction('bad')
    bad.add_effect(result, SetDifference(obj_bag, int_bag))
    bad.add_effect(done, True)
    p.add_action(bad)

    p.add_goal(done)
    return p
