"""
15-Puzzle — v2 using array reads to locate the blank (no blank tracker), instance i_short.

Preconditions read puzzle[i±1][j] / puzzle[i][j±1] directly to check for the blank.
PDDL-XTS counterpart: PDDL-XTS/15-puzzle_v2/instances/i_short.pddl
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('puzzle15_v2_i_short')

    size_t  = IntType(0, 3)
    range_t = IntType(0, 15)
    dir_t   = IntType(0, 4)

    puzzle    = Fluent('puzzle',    ArrayType(4, ArrayType(4, range_t)))
    last_move = Fluent('last_move', dir_t)
    p.add_fluent(puzzle)
    p.add_fluent(last_move, default_initial_value=0)
    p.set_initial_value(last_move, 0)

    p.set_initial_value(puzzle, [
        [ 1,  5,  2,  3],
        [ 4,  6, 10,  7],
        [ 8,  9, 11,  0],
        [12, 13, 14, 15],
    ])

    # move_up: blank is at (i-1,j), tile is at (i,j)
    act = InstantaneousAction('move_up', i=IntType(1, 3), j=size_t)
    i, j = act.parameter('i'), act.parameter('j')
    act.add_precondition(Equals(puzzle[i - 1][j], 0))
    act.add_precondition(Not(Equals(puzzle[i][j], 0)))
    act.add_precondition(Not(Equals(last_move, 2)))
    act.add_effect(puzzle[i - 1][j], puzzle[i][j])
    act.add_effect(puzzle[i][j], 0)
    act.add_effect(last_move, 1)
    p.add_action(act)

    # move_down: blank is at (i+1,j), tile is at (i,j)
    act = InstantaneousAction('move_down', i=IntType(0, 2), j=size_t)
    i, j = act.parameter('i'), act.parameter('j')
    act.add_precondition(Equals(puzzle[i + 1][j], 0))
    act.add_precondition(Not(Equals(puzzle[i][j], 0)))
    act.add_precondition(Not(Equals(last_move, 1)))
    act.add_effect(puzzle[i + 1][j], puzzle[i][j])
    act.add_effect(puzzle[i][j], 0)
    act.add_effect(last_move, 2)
    p.add_action(act)

    # move_left: blank is at (i,j-1), tile is at (i,j)
    act = InstantaneousAction('move_left', i=size_t, j=IntType(1, 3))
    i, j = act.parameter('i'), act.parameter('j')
    act.add_precondition(Equals(puzzle[i][j - 1], 0))
    act.add_precondition(Not(Equals(puzzle[i][j], 0)))
    act.add_precondition(Not(Equals(last_move, 4)))
    act.add_effect(puzzle[i][j - 1], puzzle[i][j])
    act.add_effect(puzzle[i][j], 0)
    act.add_effect(last_move, 3)
    p.add_action(act)

    # move_right: blank is at (i,j+1), tile is at (i,j)
    act = InstantaneousAction('move_right', i=size_t, j=IntType(0, 2))
    i, j = act.parameter('i'), act.parameter('j')
    act.add_precondition(Equals(puzzle[i][j + 1], 0))
    act.add_precondition(Not(Equals(puzzle[i][j], 0)))
    act.add_precondition(Not(Equals(last_move, 3)))
    act.add_effect(puzzle[i][j + 1], puzzle[i][j])
    act.add_effect(puzzle[i][j], 0)
    act.add_effect(last_move, 4)
    p.add_action(act)

    p.add_goal(Equals(puzzle, [
        [ 0,  1,  2,  3],
        [ 4,  5,  6,  7],
        [ 8,  9, 10, 11],
        [12, 13, 14, 15],
    ]))
    return p
