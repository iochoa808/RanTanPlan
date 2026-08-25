"""SetUnion of two sets with mismatched element types assigned to a set fluent. PDDL-XTS: X_set_union_type_mismatch"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_set_union_type_mismatch')

    Item  = UserType('Item')
    lvl_t = IntType(0, 5)

    x = Object('x', Item)
    y = Object('y', Item)
    p.add_objects([x, y])

    ob  = Fluent('ob',  SetType(Item))
    ib  = Fluent('ib',  SetType(lvl_t))
    res = Fluent('res', SetType(Item))
    p.add_fluent(ob,  default_initial_value=set())
    p.add_fluent(ib,  default_initial_value=set())
    p.add_fluent(res, default_initial_value=set())
    p.set_initial_value(ob,  {x})
    p.set_initial_value(ib,  {Int(1), Int(2)})
    p.set_initial_value(res, {x})

    # Error: SetUnion(ob, ib) — ob has element type Item, ib has element type IntType(0,5)
    bad = InstantaneousAction('bad')
    bad.add_effect(res, SetUnion(ob, ib))
    p.add_action(bad)

    p.add_goal(SetMember(y, res))
    return p