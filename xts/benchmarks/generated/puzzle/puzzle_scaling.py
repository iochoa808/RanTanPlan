"""
Shared generator for the N-puzzle size sweep (scale_puzzle_*).

n x n sliding-tile puzzle: ArrayType(n, ArrayType(n, IntType(0, n*n-1))).
blank_row/blank_col track the blank's position (cheaper preconditions than
array reads, and implicitly handles boundary conditions — see
PDDL-XTS/15-puzzle/domain.pddl). last_move blocks immediately undoing the
previous move.

The initial state is produced by a fixed-seed random walk of `shuffle_moves`
legal moves starting from the solved grid, which guarantees solvability
(the reverse walk is a witness plan) regardless of n. shuffle_moves is a
small constant (default 4), deliberately *not* scaled with n: this is a
horizon-based SAT planner without a puzzle-specific heuristic, so search
depth dominates runtime far more than board size does — scaling shuffle
depth with n makes even n=3 time out (verified locally). Keeping the plan
length fixed isolates what this sweep is actually for: encoding/grounding
cost (array size n*n, action parameter domains of size n) as n grows, across
both native/UP pipelines and both array encodings (theory/uf) and frame
modes (disequality/ite). Adapted from PDDL-XTS/15-puzzle/problem.py
(v1, blank-tracking).
"""
import random
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))

from unified_planning.shortcuts import *


def build_puzzle(n: int, shuffle_moves: int = 4, seed: int = 42):
    if n < 2:
        raise ValueError("n must be >= 2")

    p = Problem(f'puzzle_{n}x{n}')

    size_t  = IntType(0, n - 1)
    range_t = IntType(0, n * n - 1)
    dir_t   = IntType(0, 4)   # 0=none 1=up 2=down 3=left 4=right

    puzzle    = Fluent('puzzle',    ArrayType(n, ArrayType(n, range_t)))
    blank_row = Fluent('blank_row', size_t)
    blank_col = Fluent('blank_col', size_t)
    last_move = Fluent('last_move', dir_t)
    p.add_fluent(puzzle)
    p.add_fluent(blank_row, default_initial_value=0)
    p.add_fluent(blank_col, default_initial_value=0)
    p.add_fluent(last_move, default_initial_value=0)

    goal_grid = [[r * n + c for c in range(n)] for r in range(n)]

    # Random walk of legal moves from the solved state (blank starts at
    # (0,0)) — reversible, so the shuffled state is always solvable.
    grid = [row[:] for row in goal_grid]
    br, bc = 0, 0
    last_dir = 0
    rng = random.Random(seed)
    for _ in range(shuffle_moves):
        candidates = []  # (dir_code, new_blank_row, new_blank_col)
        if br > 0 and last_dir != 2:
            candidates.append((1, br - 1, bc))
        if br < n - 1 and last_dir != 1:
            candidates.append((2, br + 1, bc))
        if bc > 0 and last_dir != 4:
            candidates.append((3, br, bc - 1))
        if bc < n - 1 and last_dir != 3:
            candidates.append((4, br, bc + 1))
        dir_code, nr, nc = rng.choice(candidates)
        grid[br][bc], grid[nr][nc] = grid[nr][nc], grid[br][bc]
        br, bc = nr, nc
        last_dir = dir_code

    p.set_initial_value(puzzle, grid)
    p.set_initial_value(blank_row, br)
    p.set_initial_value(blank_col, bc)
    p.set_initial_value(last_move, 0)

    # move_up: tile at (i,j) slides into blank at (i-1,j)
    act = InstantaneousAction('move_up', i=IntType(1, n - 1), j=size_t)
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
    act = InstantaneousAction('move_down', i=IntType(0, n - 2), j=size_t)
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
    act = InstantaneousAction('move_left', i=size_t, j=IntType(1, n - 1))
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
    act = InstantaneousAction('move_right', i=size_t, j=IntType(0, n - 2))
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

    p.add_goal(Equals(puzzle, goal_grid))
    return p
