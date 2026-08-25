"""
Drain — remove all elements to reach an empty-set goal.
remove_elem(n): remove n from bag.
Initial: bag={1,2,3}; goal: bag={} (empty set).
Plan: remove_elem(1), remove_elem(2), remove_elem(3).
PDDL-XTS: empty_set_goal
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('drain')
    val_t = IntType(1, 3)

    bag = Fluent('bag', SetType(val_t))
    p.add_fluent(bag, default_initial_value=set())
    p.set_initial_value(bag, {Int(1), Int(2), Int(3)})

    remove_elem = InstantaneousAction('remove_elem', n=val_t)
    n = remove_elem.parameter('n')
    remove_elem.add_precondition(SetMember(n, bag))
    remove_elem.add_effect(bag, SetRemove(n, bag))
    p.add_action(remove_elem)

    p.add_goal(Equals(bag, set()))
    return p
