"""
4D hypercube whole-array goal.
PDDL-XTS: 4d_whole_goal
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('test_4d_whole')

    idx_t = IntType(0, 1)
    val_t = IntType(0, 3)
    zero_4d = [[[[0,0],[0,0]],[[0,0],[0,0]]],[[[0,0],[0,0]],[[0,0],[0,0]]]]

    hc = Fluent('hypercube', ArrayType(2, ArrayType(2, ArrayType(2, ArrayType(2, val_t)))))
    p.add_fluent(hc)
    p.set_initial_value(hc, zero_4d)

    set_act = InstantaneousAction('set', a=idx_t, b=idx_t, c=idx_t, e=idx_t, v=val_t)
    a, b, c, e, v = [set_act.parameter(x) for x in ('a','b','c','e','v')]
    set_act.add_precondition(GT(v, 0))
    set_act.add_precondition(Equals(hc[a][b][c][e], 0))
    set_act.add_effect(hc[a][b][c][e], v)
    p.add_action(set_act)

    # Goal: hc = [[[[ 1,0],[0,0]],[[0,0],[0,0]]],[[[0,0],[0,0]],[[0,2],[0,0]]]]
    target = [[[[1,0],[0,0]],[[0,0],[0,0]]],[[[0,0],[0,0]],[[0,2],[0,0]]]]
    p.add_goal(Equals(hc, target))
    return p
