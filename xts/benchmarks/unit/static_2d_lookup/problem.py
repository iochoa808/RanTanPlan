"""
Adjacency lookup — static 2D array used only in preconditions.
PDDL-XTS: static_2d_lookup
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('static_adj')

    node_t = IntType(0, 2)
    bit_t  = IntType(0, 1)

    adj = Fluent('adj', ArrayType(3, ArrayType(3, bit_t)))
    pos = Fluent('pos', node_t)
    p.add_fluent(adj)
    p.add_fluent(pos, default_initial_value=0)

    p.set_initial_value(adj, [[0,1,0],[0,0,1],[1,0,0]])
    p.set_initial_value(pos, 0)

    move = InstantaneousAction('move', frm=node_t, to=node_t)
    frm, to = move.parameter('frm'), move.parameter('to')
    move.add_precondition(Equals(pos, frm))
    move.add_precondition(Equals(adj[frm][to], 1))
    move.add_effect(pos, to)
    p.add_action(move)

    p.add_goal(Equals(pos, 2))
    return p
