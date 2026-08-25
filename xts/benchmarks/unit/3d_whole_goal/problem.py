"""
3D tensor whole-array goal — fill cells, compare whole 3D array in goal.
PDDL-XTS: 3d_whole_goal
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('test_3d_whole')

    d_t = IntType(0, 1)
    r_t = IntType(0, 1)
    c_t = IntType(0, 1)
    v_t = IntType(0, 5)

    tensor = Fluent('tensor', ArrayType(2, ArrayType(2, ArrayType(2, v_t))))
    p.add_fluent(tensor)
    p.set_initial_value(tensor, [[[0,0],[0,0]],[[0,0],[0,0]]])

    set_act = InstantaneousAction('set', d=d_t, r=r_t, c=c_t, v=v_t)
    d, r, c, v = [set_act.parameter(x) for x in ('d','r','c','v')]
    set_act.add_precondition(GT(v, 0))
    set_act.add_precondition(Equals(tensor[d][r][c], 0))
    set_act.add_effect(tensor[d][r][c], v)
    p.add_action(set_act)

    # Goal: tensor = [[[1,2],[3,0]],[[0,4],[0,0]]]
    # Plan: set(0,0,0,1), set(0,0,1,2), set(0,1,0,3), set(1,1,1,4) — 4 steps
    p.add_goal(Equals(tensor, [[[1,2],[3,0]],[[0,4],[0,0]]]))
    return p
