"""SetMember with element of wrong type (object checked against int set and vice versa). PDDL-XTS: X_sem_member_wrong_elem_type"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_sem_member_wrong_elem_type')

    Item    = UserType('Item')
    level_t = IntType(0, 9)

    item_a = Object('item_a', Item)
    item_b = Object('item_b', Item)
    p.add_objects([item_a, item_b])

    int_bag = Fluent('int_bag', SetType(level_t))
    obj_bag = Fluent('obj_bag', SetType(Item))
    found   = Fluent('found',   BoolType())
    p.add_fluent(int_bag, default_initial_value=set())
    p.add_fluent(obj_bag, default_initial_value=set())
    p.add_fluent(found,   default_initial_value=False)
    p.set_initial_value(int_bag, {Int(1), Int(5)})
    p.set_initial_value(obj_bag, {item_a, item_b})

    # Error: item_a (object) checked against int_bag (set of integers)
    find_obj_in_int = InstantaneousAction('find_obj_in_int')
    find_obj_in_int.add_precondition(SetMember(item_a, int_bag))
    find_obj_in_int.add_effect(found, True)
    p.add_action(find_obj_in_int)

    # Error: Int(5) (integer) checked against obj_bag (set of Item objects)
    find_int_in_obj = InstantaneousAction('find_int_in_obj')
    find_int_in_obj.add_precondition(SetMember(Int(5), obj_bag))
    find_int_in_obj.add_effect(found, True)
    p.add_action(find_int_in_obj)

    # Error: parameter x of type Item checked against int_bag
    scan_wrong = InstantaneousAction('scan_wrong', x=Item)
    x = scan_wrong.parameter('x')
    scan_wrong.add_precondition(SetMember(x, int_bag))
    scan_wrong.add_effect(found, True)
    p.add_action(scan_wrong)

    p.add_goal(found)
    return p