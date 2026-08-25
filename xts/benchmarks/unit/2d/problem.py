"""
2D grid — write values into a 3×4 grid, read-back goals.
PDDL-XTS: 2d
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('test_2d')

    row_t = IntType(0, 2)
    col_t = IntType(0, 3)
    val_t = IntType(0, 2)

    board = Fluent('board', ArrayType(3, ArrayType(4, val_t)))
    p.add_fluent(board)
    p.set_initial_value(board, [[0,0,0,0],[0,0,0,0],[0,0,0,0]])

    fill = InstantaneousAction('fill', i=row_t, j=col_t, v=val_t)
    i, j, v = [fill.parameter(x) for x in ('i','j','v')]
    fill.add_precondition(GT(v, 0))
    fill.add_precondition(Equals(board[i][j], 0))
    fill.add_effect(board[i][j], v)
    p.add_action(fill)

    clear = InstantaneousAction('clear', i=row_t, j=col_t)
    i, j = [clear.parameter(x) for x in ('i','j')]
    clear.add_precondition(GT(board[i][j], 0))
    clear.add_effect(board[i][j], 0)
    p.add_action(clear)

    p.add_goal(Equals(board[1][2], 2))
    p.add_goal(Equals(board[0][3], 1))
    return p
