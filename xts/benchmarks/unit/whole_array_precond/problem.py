"""
Whole-array equality used in an action PRECONDITION (not just a goal).
set2 writes cell 2, then finish fires when the whole array equals [1,2,3].
PDDL-XTS: whole_array_precond
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    em = get_environment().expression_manager
    p = Problem('whole_precond')

    val_t = IntType(0, 9)

    done = Fluent('done', BoolType())
    a    = Fluent('a', ArrayType(3, val_t))
    p.add_fluent(done, default_initial_value=False)
    p.add_fluent(a)
    p.set_initial_value(a, [1, 2, 0])

    finish = InstantaneousAction('finish')
    finish.add_precondition(Equals(a, em.Array([Int(1), Int(2), Int(3)])))
    finish.add_effect(done, True)
    p.add_action(finish)

    set2 = InstantaneousAction('set2')
    set2.add_precondition(Equals(a[Int(2)], Int(0)))
    set2.add_effect(a[Int(2)], Int(3))
    p.add_action(set2)

    p.add_goal(done)
    return p