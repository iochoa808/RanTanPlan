"""
Party lights — Count() of lit predicates gates starting the party.
PDDL-XTS: count
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('party_lights')
    Light = UserType('Light')
    l1 = Object('l1', Light)
    l2 = Object('l2', Light)
    l3 = Object('l3', Light)
    p.add_objects([l1, l2, l3])

    lit      = Fluent('lit',      l=Light)
    party_on = Fluent('party_on')
    p.add_fluent(lit,      default_initial_value=False)
    p.add_fluent(party_on, default_initial_value=False)
    p.set_initial_value(lit(l1), True)

    turn_on = InstantaneousAction('turn_on', l=Light)
    l = turn_on.parameter('l')
    turn_on.add_precondition(Not(lit(l)))
    turn_on.add_effect(lit(l), True)
    p.add_action(turn_on)

    turn_off = InstantaneousAction('turn_off', l=Light)
    l = turn_off.parameter('l')
    turn_off.add_precondition(lit(l))
    turn_off.add_effect(lit(l), False)
    p.add_action(turn_off)

    start_party = InstantaneousAction('start_party', a=Light, b=Light, c=Light)
    a, b, c = [start_party.parameter(x) for x in ('a', 'b', 'c')]
    start_party.add_precondition(Not(Equals(a, b)))
    start_party.add_precondition(Not(Equals(a, c)))
    start_party.add_precondition(Not(Equals(b, c)))
    start_party.add_precondition(GE(Count(lit(a), lit(b), lit(c)), 2))
    start_party.add_effect(party_on, True)
    p.add_action(start_party)

    p.add_goal(party_on)
    return p
