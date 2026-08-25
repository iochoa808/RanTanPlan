"""
Set membership check using object-literal set in an action precondition.
PDDL-XTS: X_set_mk_obj_in_domain

NOTE: the PDDL file is a PARSER ERROR (object names cannot appear in domain-level
set.mk expressions).  Python UP has no such restriction and will SOLVE this
problem — so the Python-source combo tests for this X_ folder will FAIL
(expected ERROR, got SOLVED).  Consider moving this folder out of X_ prefix
(like neg_bounded_bounds / nested_forall_effect) if the Python behaviour is
deemed correct.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('X_set_mk_obj_in_domain')

    Item    = UserType('Item')
    a_obj   = Object('a', Item)
    b_obj   = Object('b', Item)
    c_obj   = Object('c', Item)
    p.add_objects([a_obj, b_obj, c_obj])

    flagged = Fluent('flagged', BoolType(), x=Item)
    bag     = Fluent('bag', SetType(Item))
    p.add_fluent(flagged, default_initial_value=False)
    p.add_fluent(bag)
    p.set_initial_value(bag, {a_obj})

    flag = InstantaneousAction('flag', x=Item)
    x = flag.parameter('x')
    flag.add_precondition(SetMember(x, ObjectSet([a_obj, b_obj])))
    flag.add_effect(flagged(x), True)
    p.add_action(flag)

    p.add_goal(flagged(b_obj))
    return p