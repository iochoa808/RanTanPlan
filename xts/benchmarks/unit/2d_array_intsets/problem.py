"""
Code grid — 2D array of int-sets; propagate row-0 to row-1 for active columns.
PDDL-XTS: 2d_array_intsets
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('code_grid')

    row_t  = IntType(0, 1)
    col_t  = IntType(0, 2)
    code_t = IntType(0, 4)

    codes    = Fluent('codes',    ArrayType(2, ArrayType(3, SetType(code_t))))
    active_c = Fluent('active_c', SetType(col_t))
    p.add_fluent(codes)
    p.add_fluent(active_c, default_initial_value=set())

    p.set_initial_value(codes, [
        [{Int(1), Int(2)}, {Int(3)}, {Int(4), Int(0)}],
        [{Int(0)},         {Int(0)}, {Int(0)}],
    ])
    p.set_initial_value(active_c, {Int(0), Int(2)})

    propagate = InstantaneousAction('propagate', c=col_t)
    c = propagate.parameter('c')
    propagate.add_precondition(SetMember(c, active_c))
    propagate.add_effect(codes[1][c], codes[0][c])
    p.add_action(propagate)

    p.add_goal(SetMember(Int(1), codes[1][0]))
    p.add_goal(SetMember(Int(4), codes[1][2]))
    return p
