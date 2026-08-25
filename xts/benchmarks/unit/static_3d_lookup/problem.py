"""
Static 3D lookup table — 2×2×2 bit table used only in preconditions.
move(nd,nr,nc): allowed when reachable[nd][nr][nc]=1.
Initial: d=0,r=0,c=0, reachable[0][1][0]=1 (only this cell is 1).
Goal: r=1.  Plan: move(0,1,0)  →  r=1.
PDDL-XTS: static_3d_lookup
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('static_3d')
    bit_t = IntType(0, 1)
    d_t   = IntType(0, 1)
    r_t   = IntType(0, 1)
    c_t   = IntType(0, 1)

    reachable = Fluent('reachable', ArrayType(2, ArrayType(2, ArrayType(2, bit_t))))
    d         = Fluent('d', d_t)
    r         = Fluent('r', r_t)
    c         = Fluent('c', c_t)
    p.add_fluent(reachable)
    p.add_fluent(d, default_initial_value=0)
    p.add_fluent(r, default_initial_value=0)
    p.add_fluent(c, default_initial_value=0)

    # reachable[d][r][c]: only [0][1][0] = 1
    p.set_initial_value(reachable, [
        [[0, 0], [1, 0]],
        [[0, 0], [0, 0]],
    ])
    p.set_initial_value(d, 0)
    p.set_initial_value(r, 0)
    p.set_initial_value(c, 0)

    move = InstantaneousAction('move', nd=d_t, nr=r_t, nc=c_t)
    nd, nr, nc = move.parameter('nd'), move.parameter('nr'), move.parameter('nc')
    move.add_precondition(Equals(reachable[nd][nr][nc], 1))
    move.add_effect(d, nd)
    move.add_effect(r, nr)
    move.add_effect(c, nc)
    p.add_action(move)

    p.add_goal(Equals(r, 1))
    return p
