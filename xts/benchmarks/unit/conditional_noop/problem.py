"""
Conditional noop — guard is false; conditional write must NOT fire.
PDDL-XTS: conditional_noop
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('cond_noop')

    val_t = IntType(0, 9)
    guard = Fluent('guard')
    board = Fluent('board', ArrayType(2, val_t))
    p.add_fluent(guard, default_initial_value=False)
    p.add_fluent(board)

    p.set_initial_value(board, [0, 0])
    # guard NOT set → False

    fire = InstantaneousAction('fire')
    fire.add_precondition(Equals(board[1], 0))
    fire.add_effect(board[1], 3)
    fire.add_effect(board[0], 9, condition=guard)
    p.add_action(fire)

    p.add_goal(Equals(board[1], 3))
    p.add_goal(Equals(board[0], 0))
    return p
