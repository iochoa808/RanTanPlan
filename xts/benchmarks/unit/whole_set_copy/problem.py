"""
Archive — whole-set copy: assign one set fluent to another.
archive copies original → backup; flush clears original.
Initial: original={1,2,3}, backup={}.  Goal: 1,2,3 ∈ backup  AND  original={}.
Plan: archive, flush.
PDDL-XTS: whole_set_copy
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('archive')

    val_t = IntType(0, 4)

    original = Fluent('original', SetType(val_t))
    backup   = Fluent('backup',   SetType(val_t))
    p.add_fluent(original, default_initial_value=set())
    p.add_fluent(backup,   default_initial_value=set())

    p.set_initial_value(original, {Int(1), Int(2), Int(3)})
    p.set_initial_value(backup,   set())

    # archive: assign backup = original  (whole-set copy)
    archive = InstantaneousAction('archive')
    archive.add_precondition(Equals(SetCardinality(backup), 0))
    archive.add_effect(backup, original)
    p.add_action(archive)

    # flush: clear original to empty set
    flush = InstantaneousAction('flush')
    flush.add_precondition(GE(SetCardinality(backup), 1))
    flush.add_effect(original, SetDifference(original, original))
    p.add_action(flush)

    p.add_goal(SetMember(Int(1), backup))
    p.add_goal(SetMember(Int(2), backup))
    p.add_goal(SetMember(Int(3), backup))
    p.add_goal(Equals(SetCardinality(original), 0))
    return p