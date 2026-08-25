"""
4D array (4×3×2×1) — non-uniform dimensions, index ordering test.
PDDL-XTS: 4d_index_order
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('test_4d_order')

    d0_t = IntType(0, 3)  # size 4
    d1_t = IntType(0, 2)  # size 3
    d2_t = IntType(0, 1)  # size 2
    d3_t = IntType(0, 0)  # size 1
    val_t = IntType(0, 3)

    zero = [[ [[0],[0]] for _ in range(3)] for _ in range(4)]

    arr = Fluent('arr', ArrayType(4, ArrayType(3, ArrayType(2, ArrayType(1, val_t)))))
    p.add_fluent(arr)
    p.set_initial_value(arr, zero)

    set_act = InstantaneousAction('set', a=d0_t, b=d1_t, c=d2_t, e=d3_t, v=val_t)
    a, b, c, e, v = [set_act.parameter(x) for x in ('a','b','c','e','v')]
    set_act.add_precondition(GT(v, 0))
    set_act.add_precondition(Equals(arr[a][b][c][e], 0))
    set_act.add_effect(arr[a][b][c][e], v)
    p.add_action(set_act)

    clear_act = InstantaneousAction('clear', a=d0_t, b=d1_t, c=d2_t, e=d3_t)
    a, b, c, e = [clear_act.parameter(x) for x in ('a','b','c','e')]
    clear_act.add_precondition(GT(arr[a][b][c][e], 0))
    clear_act.add_effect(arr[a][b][c][e], 0)
    p.add_action(clear_act)

    p.add_goal(Equals(arr[3][0][0][0], 1))
    p.add_goal(Equals(arr[0][2][0][0], 2))
    p.add_goal(Equals(arr[0][0][1][0], 3))
    return p
