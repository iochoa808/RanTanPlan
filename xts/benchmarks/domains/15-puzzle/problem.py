"""
15-Puzzle — v1 with blank-position tracking, instance i_short.

puzzle: ArrayType(4, ArrayType(4, IntType(0,15)))
blank_row, blank_col track blank position; last_move prevents reversal.
PDDL-XTS counterpart: PDDL-XTS/15-puzzle/instances/i_short.pddl
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('puzzle15_i_short')

    size_t  = IntType(0, 3)
    range_t = IntType(0, 15)
    dir_t   = IntType(0, 4)   # 0=none 1=up 2=down 3=left 4=right

    puzzle    = Fluent('puzzle',    ArrayType(4, ArrayType(4, range_t)))
    blank_row = Fluent('blank_row', size_t)
    blank_col = Fluent('blank_col', size_t)
    last_move = Fluent('last_move', dir_t)
    p.add_fluent(puzzle)
    p.add_fluent(blank_row, default_initial_value=0)
    p.add_fluent(blank_col, default_initial_value=0)
    p.add_fluent(last_move, default_initial_value=0)

    p.set_initial_value(puzzle, [
        [ 1,  5,  2,  3],
        [ 4,  6, 10,  7],
        [ 8,  9, 11,  0],
        [12, 13, 14, 15],
    ])
    p.set_initial_value(blank_row, 2)
    p.set_initial_value(blank_col, 3)
    p.set_initial_value(last_move, 0)

    # move_up: tile at (i,j) slides into blank at (i-1,j)
    act = InstantaneousAction('move_up', i=IntType(1, 3), j=size_t)
    i, j = act.parameter('i'), act.parameter('j')
    act.add_precondition(Equals(blank_row, i - 1))
    act.add_precondition(Equals(blank_col, j))
    act.add_precondition(Not(Equals(last_move, 2)))
    act.add_effect(puzzle[i - 1][j], puzzle[i][j])
    act.add_effect(puzzle[i][j], 0)
    act.add_effect(blank_row, i)
    act.add_effect(blank_col, j)
    act.add_effect(last_move, 1)
    p.add_action(act)

    # move_down: tile at (i,j) slides into blank at (i+1,j)
    act = InstantaneousAction('move_down', i=IntType(0, 2), j=size_t)
    i, j = act.parameter('i'), act.parameter('j')
    act.add_precondition(Equals(blank_row, i + 1))
    act.add_precondition(Equals(blank_col, j))
    act.add_precondition(Not(Equals(last_move, 1)))
    act.add_effect(puzzle[i + 1][j], puzzle[i][j])
    act.add_effect(puzzle[i][j], 0)
    act.add_effect(blank_row, i)
    act.add_effect(blank_col, j)
    act.add_effect(last_move, 2)
    p.add_action(act)

    # move_left: tile at (i,j) slides into blank at (i,j-1)
    act = InstantaneousAction('move_left', i=size_t, j=IntType(1, 3))
    i, j = act.parameter('i'), act.parameter('j')
    act.add_precondition(Equals(blank_row, i))
    act.add_precondition(Equals(blank_col, j - 1))
    act.add_precondition(Not(Equals(last_move, 4)))
    act.add_effect(puzzle[i][j - 1], puzzle[i][j])
    act.add_effect(puzzle[i][j], 0)
    act.add_effect(blank_row, i)
    act.add_effect(blank_col, j)
    act.add_effect(last_move, 3)
    p.add_action(act)

    # move_right: tile at (i,j) slides into blank at (i,j+1)
    act = InstantaneousAction('move_right', i=size_t, j=IntType(0, 2))
    i, j = act.parameter('i'), act.parameter('j')
    act.add_precondition(Equals(blank_row, i))
    act.add_precondition(Equals(blank_col, j + 1))
    act.add_precondition(Not(Equals(last_move, 3)))
    act.add_effect(puzzle[i][j + 1], puzzle[i][j])
    act.add_effect(puzzle[i][j], 0)
    act.add_effect(blank_row, i)
    act.add_effect(blank_col, j)
    act.add_effect(last_move, 4)
    p.add_action(act)

    p.add_goal(Equals(puzzle, [
        [ 0,  1,  2,  3],
        [ 4,  5,  6,  7],
        [ 8,  9, 10, 11],
        [12, 13, 14, 15],
    ]))
    return p
