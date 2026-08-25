"""
Number chain — arithmetic in add AND remove: grow a chain then trim its head.
step(n): add (n+1) when n is present.
trim(n): remove (n+1) when both n and n+1 are present.
Initial chain = {0, 4}; goal: 3 ∈ chain AND 4 ∉ chain.
Plan: step(0), step(1), step(2), trim(3)  →  chain = {0,1,2,3} → trim(3) removes 4.
PDDL-XTS: add_arithmetic
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('number_chain')
    chain = Fluent('chain', SetType(IntType(0, 5)))
    p.add_fluent(chain, default_initial_value=set())
    p.set_initial_value(chain, {Int(0), Int(4)})

    step = InstantaneousAction('step', n=IntType(0, 5))
    n = step.parameter('n')
    step.add_precondition(LT(n, 4))
    step.add_precondition(SetMember(n, chain))
    step.add_precondition(Not(SetMember(Plus(n, 1), chain)))
    step.add_effect(chain, SetAdd(Plus(n, 1), chain))
    p.add_action(step)

    # trim(n): remove (n+1) — arithmetic expression in SetRemove; n capped at 4 so n+1 ≤ 5
    trim = InstantaneousAction('trim', n=IntType(0, 4))
    n2 = trim.parameter('n')
    trim.add_precondition(SetMember(n2, chain))
    trim.add_precondition(SetMember(Plus(n2, 1), chain))
    trim.add_effect(chain, SetRemove(Plus(n2, 1), chain))
    p.add_action(trim)

    p.add_goal(SetMember(Int(3), chain))
    p.add_goal(Not(SetMember(Int(4), chain)))
    return p
