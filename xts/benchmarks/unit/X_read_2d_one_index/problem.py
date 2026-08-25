"""Single index on 2D array returns sub-array; assigning sub-array to scalar and vice versa. PDDL-XTS: X_read_2d_one_index"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_read_2d_one_index')

    row_t = IntType(0, 2)
    val_t = IntType(0, 9)

    board  = Fluent('board',  ArrayType(3, ArrayType(3, val_t)))
    result = Fluent('result', val_t)
    p.add_fluent(board)
    p.add_fluent(result, default_initial_value=0)
    p.set_initial_value(board, [[1,2,3],[4,5,6],[7,8,9]])

    # Error: board[i] is a 1D-array expression; assigning it to a scalar result → type error
    single_read = InstantaneousAction('single_read', i=row_t)
    i = single_read.parameter('i')
    single_read.add_effect(result, board[i])
    p.add_action(single_read)

    # Error: board[i] is a 1D-array expression; assigning scalar 9 to an array row → type error
    single_write = InstantaneousAction('single_write', i=row_t)
    i2 = single_write.parameter('i')
    single_write.add_effect(board[i2], 9)
    p.add_action(single_write)

    p.add_goal(Equals(result, 5))
    return p