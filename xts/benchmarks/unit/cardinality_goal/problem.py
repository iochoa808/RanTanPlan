"""
Exact-size bag — cardinality = n (equality form) used directly in goal.
fill adds elements; goal requires exactly 3 elements in the bag.
Also tests: cardinality = 3 (equality comparison, not just >= or <).
Initial: bag={1}.  Goal: cardinality(bag) = 3.
Plan: fill(2), fill(3).
PDDL-XTS: cardinality_goal
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('exact_bag')

    val_t = IntType(0, 5)

    bag = Fluent('bag', SetType(val_t))
    p.add_fluent(bag, default_initial_value=set())
    p.set_initial_value(bag, {Int(1)})

    fill = InstantaneousAction('fill', n=val_t)
    n = fill.parameter('n')
    fill.add_precondition(Not(SetMember(n, bag)))
    fill.add_precondition(LT(SetCardinality(bag), 3))
    fill.add_effect(bag, SetAdd(n, bag))
    p.add_action(fill)

    # goal: cardinality = 3  (equality form of cardinality comparison)
    p.add_goal(Equals(SetCardinality(bag), Int(3)))
    return p