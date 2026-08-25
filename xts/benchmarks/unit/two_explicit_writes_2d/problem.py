"""
Constant-index writes on a 2D array — multi-bracket IPAR cell SVs ("board[0][1]").
Sequenced via preconditions so frame axioms must carry 2D cell values.
PDDL-XTS: two_explicit_writes_2d
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('two_explicit_writes_2d')
    val_t = IntType(0, 9)

    board = Fluent('board', ArrayType(2, ArrayType(3, val_t)))
    p.add_fluent(board)
    p.set_initial_value(board, [[0, 0, 0], [0, 0, 0]])

    write_a = InstantaneousAction('write_a')
    write_a.add_precondition(Equals(board[0][1], 0))
    write_a.add_effect(board[0][1], 5)
    p.add_action(write_a)

    write_b = InstantaneousAction('write_b')
    write_b.add_precondition(Equals(board[0][1], 5))
    write_b.add_precondition(Equals(board[1][2], 0))
    write_b.add_effect(board[1][2], 7)
    p.add_action(write_b)

    p.add_goal(Equals(board[0][1], 5))
    p.add_goal(Equals(board[1][2], 7))
    p.add_goal(Equals(board[1][0], 0))
    return p
