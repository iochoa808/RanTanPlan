"""
Cold lab — set operations with bounded-int temperature gate.
PDDL-XTS: sets3
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('cold_lab')

    Sample = UserType('Sample')
    a_, b_, c_, d_, e_ = [Object(n, Sample) for n in ('a','b','c','d','e')]
    p.add_objects([a_, b_, c_, d_, e_])

    temp_t = IntType(0, 5)
    lab     = Fluent('lab',     SetType(Sample))
    archive = Fluent('archive', SetType(Sample))
    temp    = Fluent('temp',    temp_t)
    p.add_fluent(lab,     default_initial_value=set())
    p.add_fluent(archive, default_initial_value=set())
    p.add_fluent(temp,    default_initial_value=0)

    p.set_initial_value(lab,     {b_, c_, e_})
    p.set_initial_value(archive, {a_, b_, c_})
    p.set_initial_value(temp,    4)

    load = InstantaneousAction('load', s=Sample)
    s = load.parameter('s')
    load.add_precondition(Not(SetMember(s, lab)))
    load.add_precondition(LE(temp, 2))
    load.add_effect(lab, SetAdd(s, lab))
    p.add_action(load)

    unload = InstantaneousAction('unload', s=Sample)
    s = unload.parameter('s')
    unload.add_precondition(SetMember(s, lab))
    unload.add_precondition(LE(temp, 2))
    unload.add_effect(lab, SetRemove(s, lab))
    p.add_action(unload)

    cool = InstantaneousAction('cool')
    cool.add_precondition(GT(temp, 0))
    cool.add_effect(temp, Minus(temp, 1))
    p.add_action(cool)

    warm = InstantaneousAction('warm')
    warm.add_precondition(LT(temp, 5))
    warm.add_effect(temp, Plus(temp, 1))
    p.add_action(warm)

    trim_to_archive = InstantaneousAction('trim_to_archive')
    trim_to_archive.add_precondition(GE(SetCardinality(lab), 3))
    trim_to_archive.add_effect(lab, SetIntersection(lab, archive))
    p.add_action(trim_to_archive)

    complete_from_archive = InstantaneousAction('complete_from_archive')
    complete_from_archive.add_precondition(SetSubseteq(lab, archive))
    complete_from_archive.add_effect(lab, SetUnion(lab, archive))
    p.add_action(complete_from_archive)

    emergency_fill = InstantaneousAction('emergency_fill')
    emergency_fill.add_precondition(SetDisjoint(lab, archive))
    emergency_fill.add_precondition(LE(temp, 1))
    emergency_fill.add_effect(lab, SetUnion(lab, archive))
    p.add_action(emergency_fill)

    p.add_goal(Equals(lab,  {a_, b_, c_}))
    p.add_goal(Equals(temp, 1))
    return p
