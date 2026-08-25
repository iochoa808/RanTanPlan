"""
3D tensor — read/write a 2×2×2 integer tensor.
PDDL-XTS: 3d
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('test_3d')

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

    clear_act = InstantaneousAction('clear', d=d_t, r=r_t, c=c_t)
    d, r, c = [clear_act.parameter(x) for x in ('d','r','c')]
    clear_act.add_precondition(GT(tensor[d][r][c], 0))
    clear_act.add_effect(tensor[d][r][c], 0)
    p.add_action(clear_act)

    p.add_goal(Equals(tensor[0][1][0], 3))
    p.add_goal(Equals(tensor[1][0][1], 2))
    return p
