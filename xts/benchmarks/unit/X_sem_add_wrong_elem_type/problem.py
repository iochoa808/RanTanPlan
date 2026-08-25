"""SetAdd with element of wrong type (object into int set; int into object set). PDDL-XTS: X_sem_add_wrong_elem_type"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_sem_add_wrong_elem_type')

    Item    = UserType('Item')
    level_t = IntType(0, 9)

    item_a = Object('item_a', Item)
    item_b = Object('item_b', Item)
    p.add_objects([item_a, item_b])

    int_bag = Fluent('int_bag', SetType(level_t))
    obj_bag = Fluent('obj_bag', SetType(Item))
    p.add_fluent(int_bag, default_initial_value=set())
    p.add_fluent(obj_bag, default_initial_value=set())
    p.set_initial_value(int_bag, {Int(1), Int(3)})
    p.set_initial_value(obj_bag, {item_a})

    # Error: item_a is an object, not an integer — wrong element type for int_bag
    put_obj_in_int_set = InstantaneousAction('put_obj_in_int_set')
    put_obj_in_int_set.add_effect(int_bag, SetAdd(item_a, int_bag))
    p.add_action(put_obj_in_int_set)

    # Error: Int(5) is an integer, not an Item — wrong element type for obj_bag
    put_int_in_obj_set = InstantaneousAction('put_int_in_obj_set')
    put_int_in_obj_set.add_effect(obj_bag, SetAdd(Int(5), obj_bag))
    p.add_action(put_int_in_obj_set)

    p.add_goal(SetMember(item_a, int_bag))
    return p