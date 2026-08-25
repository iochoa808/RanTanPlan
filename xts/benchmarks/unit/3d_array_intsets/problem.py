"""
Code cube — 3D array of int-sets; propagate layer-0 to layer-1.
Initial: cube[0][0][0]={1,2}, cube[0][0][1]={3}, others={}.
Goal: 2 ∈ cube[1][0][0].
Plan: propagate(0, 0)  →  cube[1][0][0]={1,2}.
PDDL-XTS: 3d_array_intsets
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('code_cube')

    row_t  = IntType(0, 1)
    col_t  = IntType(0, 1)
    code_t = IntType(0, 4)

    cube = Fluent('cube', ArrayType(2, ArrayType(2, ArrayType(2, SetType(code_t)))))
    p.add_fluent(cube)

    p.set_initial_value(cube, [
        [[{Int(1), Int(2)}, {Int(3)}],
         [set(),            set()         ]],
        [[set(),            set()         ],
         [set(),            set()         ]],
    ])

    propagate = InstantaneousAction('propagate', r=row_t, c=col_t)
    r = propagate.parameter('r')
    c = propagate.parameter('c')
    propagate.add_effect(cube[1][r][c], cube[0][r][c])
    p.add_action(propagate)

    p.add_goal(SetMember(Int(2), cube[1][0][0]))
    return p
