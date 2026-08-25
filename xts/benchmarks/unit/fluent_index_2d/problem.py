"""
Score grid — read-modify-write on a 2×3 grid cell.
PDDL-XTS: fluent_index_2d
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('score_grid')

    row_t   = IntType(0, 1)
    col_t   = IntType(0, 2)
    score_t = IntType(0, 5)

    board = Fluent('board', ArrayType(2, ArrayType(3, score_t)))
    p.add_fluent(board)
    p.set_initial_value(board, [[1, 3, 0], [2, 0, 4]])

    inc = InstantaneousAction('inc', i=row_t, j=col_t)
    i, j = inc.parameter('i'), inc.parameter('j')
    inc.add_precondition(LT(board[i][j], 5))
    inc.add_effect(board[i][j], Plus(board[i][j], 1))
    p.add_action(inc)

    p.add_goal(Equals(board[1][1], 3))
    return p
