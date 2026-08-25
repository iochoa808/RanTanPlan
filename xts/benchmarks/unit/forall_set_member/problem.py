"""
Full bag — forall over integer range with set membership in body.
verify fires only when every integer in [0..4] is in the bag
(forall ?i in (0..4): ?i ∈ bag).
Initial: bag={0,2,4}.  Goal: verified=True.
Plan: fill(1), fill(3), verify.
PDDL-XTS: forall_set_member
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('full_bag')

    val_t = IntType(0, 4)

    bag      = Fluent('bag',      SetType(val_t))
    verified = Fluent('verified')
    p.add_fluent(bag,      default_initial_value=set())
    p.add_fluent(verified, default_initial_value=False)

    p.set_initial_value(bag,      {Int(0), Int(2), Int(4)})
    p.set_initial_value(verified, False)

    fill = InstantaneousAction('fill', n=val_t)
    n = fill.parameter('n')
    fill.add_precondition(Not(SetMember(n, bag)))
    fill.add_effect(bag, SetAdd(n, bag))
    p.add_action(fill)

    # verify: forall ?i in (0..4): member(?i, bag)
    rv = RangeVariable('i', 0, 4)
    verify = InstantaneousAction('verify')
    verify.add_precondition(Not(verified))
    verify.add_precondition(Forall(SetMember(rv, bag), rv))
    verify.add_effect(verified, True)
    p.add_action(verify)

    p.add_goal(verified)
    return p