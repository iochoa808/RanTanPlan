"""
Tetris (Sequential Optimal) — minimal 2x2 instance.
Features: 2D array (board), bounded int (srow/scol).
Board: [[1,0],[0,0]], goal: board[1][1]=1.
Minimal plan: move-down(0,0), move-right(1,0) — 2 steps.
PDDL-XTS counterpart: xts/benchmarks/translations/tetris-sequential-optimal/
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('minimal-tetris-xts')

    size_t = IntType(0, 1)
    cell_t = IntType(0, 1)

    board = Fluent('board', ArrayType(2, ArrayType(2, cell_t)))
    srow  = Fluent('srow', size_t)
    scol  = Fluent('scol', size_t)

    p.add_fluent(board)
    p.add_fluent(srow, default_initial_value=Int(0))
    p.add_fluent(scol, default_initial_value=Int(0))

    # Initial: square at (0,0)
    p.set_initial_value(board, [[1, 0], [0, 0]])
    p.set_initial_value(srow, Int(0))
    p.set_initial_value(scol, Int(0))

    # move-down: (sr,sc) -> (sr+1,sc)
    move_down = InstantaneousAction('move-down', sr=size_t, sc=size_t)
    sr, sc = move_down.parameter('sr'), move_down.parameter('sc')
    move_down.add_precondition(Equals(srow, sr))
    move_down.add_precondition(Equals(scol, sc))
    move_down.add_precondition(Equals(board[Plus(sr, Int(1))][sc], Int(0)))
    move_down.add_effect(board[sr][sc], Int(0))
    move_down.add_effect(board[Plus(sr, Int(1))][sc], Int(1))
    move_down.add_effect(srow, Plus(sr, Int(1)))
    p.add_action(move_down)

    # move-up: (sr,sc) -> (sr-1,sc)
    move_up = InstantaneousAction('move-up', sr=size_t, sc=size_t)
    sr, sc = move_up.parameter('sr'), move_up.parameter('sc')
    move_up.add_precondition(Equals(srow, sr))
    move_up.add_precondition(Equals(scol, sc))
    move_up.add_precondition(Equals(board[Minus(sr, Int(1))][sc], Int(0)))
    move_up.add_effect(board[sr][sc], Int(0))
    move_up.add_effect(board[Minus(sr, Int(1))][sc], Int(1))
    move_up.add_effect(srow, Minus(sr, Int(1)))
    p.add_action(move_up)

    # move-right: (sr,sc) -> (sr,sc+1)
    move_right = InstantaneousAction('move-right', sr=size_t, sc=size_t)
    sr, sc = move_right.parameter('sr'), move_right.parameter('sc')
    move_right.add_precondition(Equals(srow, sr))
    move_right.add_precondition(Equals(scol, sc))
    move_right.add_precondition(Equals(board[sr][Plus(sc, Int(1))], Int(0)))
    move_right.add_effect(board[sr][sc], Int(0))
    move_right.add_effect(board[sr][Plus(sc, Int(1))], Int(1))
    move_right.add_effect(scol, Plus(sc, Int(1)))
    p.add_action(move_right)

    # move-left: (sr,sc) -> (sr,sc-1)
    move_left = InstantaneousAction('move-left', sr=size_t, sc=size_t)
    sr, sc = move_left.parameter('sr'), move_left.parameter('sc')
    move_left.add_precondition(Equals(srow, sr))
    move_left.add_precondition(Equals(scol, sc))
    move_left.add_precondition(Equals(board[sr][Minus(sc, Int(1))], Int(0)))
    move_left.add_effect(board[sr][sc], Int(0))
    move_left.add_effect(board[sr][Minus(sc, Int(1))], Int(1))
    move_left.add_effect(scol, Minus(sc, Int(1)))
    p.add_action(move_left)

    # Goal: square at (1,1)
    p.add_goal(Equals(board[Int(1)][Int(1)], Int(1)))

    return p
