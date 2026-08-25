"""
Whole-array ASSIGN of a 2D array (nested constant value), then a constant
cell write on the replaced array in the next step.
PDDL-XTS: whole_2d_replace_then_write
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    em = get_environment().expression_manager
    p = Problem('whole_2d_replace_then_write')
    val_t = IntType(0, 9)

    pad = Fluent('pad', ArrayType(2, ArrayType(2, val_t)))
    p.add_fluent(pad)
    p.set_initial_value(pad, [[0, 0], [0, 0]])

    load = InstantaneousAction('load')
    load.add_precondition(Equals(pad[0][0], 0))
    load.add_effect(pad, em.Array([em.Array([Int(1), Int(2)]),
                                   em.Array([Int(3), Int(4)])]))
    p.add_action(load)

    touch = InstantaneousAction('touch')
    touch.add_precondition(Equals(pad[0][0], 1))
    touch.add_effect(pad[1][1], 9)
    p.add_action(touch)

    p.add_goal(Equals(pad[0][0], 1))
    p.add_goal(Equals(pad[0][1], 2))
    p.add_goal(Equals(pad[1][0], 3))
    p.add_goal(Equals(pad[1][1], 9))
    return p
