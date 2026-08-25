"""
Set nested — nested union effects (union of union).
PDDL-XTS: sets_nested
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('set_nested_effects')

    level_t = IntType(0, 5)
    bucket_a = Fluent('bucket_a', SetType(level_t))
    bucket_b = Fluent('bucket_b', SetType(level_t))
    bucket_c = Fluent('bucket_c', SetType(level_t))
    result   = Fluent('result',   SetType(level_t))
    for f in [bucket_a, bucket_b, bucket_c, result]:
        p.add_fluent(f, default_initial_value=set())
    p.set_initial_value(bucket_a, set())
    p.set_initial_value(bucket_b, set())
    p.set_initial_value(bucket_c, set())
    p.set_initial_value(result,   set())

    merge_all = InstantaneousAction('merge_all')
    merge_all.add_effect(result, SetUnion(SetUnion(bucket_a, bucket_b), bucket_c))
    p.add_action(merge_all)

    for fname, fluent in [('a', bucket_a), ('b', bucket_b), ('c', bucket_c)]:
        act = InstantaneousAction(f'add_to_{fname}', x=level_t)
        x = act.parameter('x')
        act.add_precondition(Not(SetMember(x, fluent)))
        act.add_effect(fluent, SetAdd(x, fluent))
        p.add_action(act)

    p.add_goal(SetMember(Int(1), result))
    p.add_goal(SetMember(Int(2), result))
    p.add_goal(SetMember(Int(3), result))
    return p
