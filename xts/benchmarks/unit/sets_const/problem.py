"""
Set constant — set literal (set.mk) in precondition and cardinality comparison.
PDDL-XTS: sets_const
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('set_constant_test')

    level_t = IntType(0, 9)
    active  = Fluent('active',  SetType(level_t))
    allowed = Fluent('allowed', SetType(level_t))
    p.add_fluent(active,  default_initial_value=set())
    p.add_fluent(allowed, default_initial_value=set())

    even_set = {Int(2), Int(4), Int(6)}
    p.set_initial_value(active,  set())
    p.set_initial_value(allowed, even_set)

    activate_even = InstantaneousAction('activate_even', a=level_t)
    a = activate_even.parameter('a')
    activate_even.add_precondition(SetMember(a, even_set))
    activate_even.add_precondition(Not(SetMember(a, active)))
    activate_even.add_effect(active, SetAdd(a, active))
    p.add_action(activate_even)

    deactivate = InstantaneousAction('deactivate', a=level_t)
    a = deactivate.parameter('a')
    deactivate.add_precondition(SetMember(a, active))
    deactivate.add_effect(active, SetRemove(a, active))
    p.add_action(deactivate)

    mark_complete = InstantaneousAction('mark_complete')
    mark_complete.add_precondition(GE(SetCardinality(active), SetCardinality(even_set)))
    mark_complete.add_effect(active, SetUnion(active, allowed))
    p.add_action(mark_complete)

    p.add_goal(SetMember(Int(2), active))
    p.add_goal(SetMember(Int(4), active))
    p.add_goal(SetMember(Int(6), active))
    return p
