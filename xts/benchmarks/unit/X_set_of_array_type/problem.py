"""Set whose element type is an array — not supported. PDDL-XTS: X_set_of_array_type"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_set_of_array')

    val_t   = IntType(0, 4)
    arr_t   = ArrayType(3, val_t)

    # Error: SetType(ArrayType(...)) — set of arrays is not supported
    arrays = Fluent('arrays', SetType(arr_t))
    done   = Fluent('done',   BoolType())
    p.add_fluent(arrays, default_initial_value=set())
    p.add_fluent(done,   default_initial_value=False)

    check = InstantaneousAction('check')
    check.add_precondition(Not(done))
    check.add_effect(done, True)
    p.add_action(check)

    p.add_goal(done)
    return p
