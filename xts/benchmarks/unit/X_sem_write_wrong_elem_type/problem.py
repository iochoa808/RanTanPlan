"""Writing wrong element type into array: object into int array and int into object array. PDDL-XTS: X_sem_write_wrong_elem_type"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_sem_write_wrong_elem_type')

    slot_t = IntType(0, 2)
    val_t  = IntType(0, 4)
    Token  = UserType('Token')

    tok_a  = Object('tok_a',  Token)
    tok_b  = Object('tok_b',  Token)
    tok_c  = Object('tok_c',  Token)
    item_a = Object('item_a', Token)
    p.add_objects([tok_a, tok_b, tok_c, item_a])

    int_arr = Fluent('int_arr', ArrayType(3, val_t))
    obj_arr = Fluent('obj_arr', ArrayType(3, Token))
    p.add_fluent(int_arr)
    p.add_fluent(obj_arr)
    p.set_initial_value(int_arr, [0, 0, 0])
    p.set_initial_value(obj_arr, [tok_a, tok_b, tok_c])

    # Error: item_a is a Token object, not an integer — wrong type for int_arr
    cross_write_obj = InstantaneousAction('cross_write_obj', i=slot_t)
    i = cross_write_obj.parameter('i')
    cross_write_obj.add_effect(int_arr[i], item_a)
    p.add_action(cross_write_obj)

    # Error: Int(5) is an integer, not a Token — wrong type for obj_arr
    cross_write_int = InstantaneousAction('cross_write_int', i=slot_t)
    i2 = cross_write_int.parameter('i')
    cross_write_int.add_effect(obj_arr[i2], Int(5))
    p.add_action(cross_write_int)

    p.add_goal(Equals(int_arr[Int(0)], 1))
    return p