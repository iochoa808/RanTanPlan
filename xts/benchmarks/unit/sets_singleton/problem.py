"""
Token ring — singleton set: release then claim.
PDDL-XTS: sets_singleton
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('sets_singleton')

    Node = UserType('Node')
    n0 = Object('n0', Node)
    n1 = Object('n1', Node)
    n2 = Object('n2', Node)
    p.add_objects([n0, n1, n2])

    current = Fluent('current', SetType(Node))
    p.add_fluent(current, default_initial_value=set())
    p.set_initial_value(current, {n0})

    release = InstantaneousAction('release', frm=Node)
    frm = release.parameter('frm')
    release.add_precondition(SetMember(frm, current))
    release.add_effect(current, SetRemove(frm, current))
    p.add_action(release)

    claim = InstantaneousAction('claim', to=Node)
    to = claim.parameter('to')
    claim.add_precondition(Not(SetMember(to, current)))
    claim.add_effect(current, SetAdd(to, current))
    p.add_action(claim)

    p.add_goal(SetMember(n2, current))
    return p
