"""Set operations (subseteq, union, intersection, disjoint) applied to sets of mismatched types. PDDL-XTS: X_set_ops_type_mismatch"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_set_ops_type_mismatch')

    Item    = UserType('Item')
    level_t = IntType(0, 5)

    item_a = Object('item_a', Item)
    item_b = Object('item_b', Item)
    p.add_objects([item_a, item_b])

    obj_bag    = Fluent('obj_bag',    SetType(Item))
    int_bag    = Fluent('int_bag',    SetType(level_t))
    result_obj = Fluent('result_obj', SetType(Item))
    result_int = Fluent('result_int', SetType(level_t))
    done       = Fluent('done',       BoolType())
    p.add_fluent(obj_bag,    default_initial_value=set())
    p.add_fluent(int_bag,    default_initial_value=set())
    p.add_fluent(result_obj, default_initial_value=set())
    p.add_fluent(result_int, default_initial_value=set())
    p.add_fluent(done,       default_initial_value=False)
    p.set_initial_value(obj_bag, {item_a, item_b})
    p.set_initial_value(int_bag, {Int(1), Int(3)})

    # Error: SetSubseteq requires both operands to have the same element type
    check_subset = InstantaneousAction('check_subset')
    check_subset.add_precondition(SetSubseteq(obj_bag, int_bag))
    check_subset.add_effect(done, True)
    p.add_action(check_subset)

    # Error: SetUnion requires both operands to have the same element type
    do_union = InstantaneousAction('do_union')
    do_union.add_effect(result_obj, SetUnion(obj_bag, int_bag))
    p.add_action(do_union)

    # Error: SetIntersection requires both operands to have the same element type
    do_intersect = InstantaneousAction('do_intersect')
    do_intersect.add_effect(result_int, SetIntersection(obj_bag, int_bag))
    p.add_action(do_intersect)

    # Error: SetDisjoint requires both operands to have the same element type
    check_disjoint = InstantaneousAction('check_disjoint')
    check_disjoint.add_precondition(SetDisjoint(obj_bag, int_bag))
    check_disjoint.add_effect(done, True)
    p.add_action(check_disjoint)

    p.add_goal(done)
    return p