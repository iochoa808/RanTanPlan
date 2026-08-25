"""
Labyrinth (small 2×2) — 2D array of objects + object fluent.

Features: ArrayType(2, ArrayType(2, Card)), object-type fluent (robot_at),
          boolean predicate on objects (connections), bounded-int action params.
Adapted from ~/unified-planning/docs/extensions/domains/labyrinth_v2/Labyrinth.py
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))

from unified_planning.shortcuts import *


def get_problem():
    n = 2
    n_cards = n * n

    # Tile layout (card ids):
    #   0 1
    #   2 3
    # Paths: robot starts at card_0 (top-left), goal = reach card_3 (bottom-right)
    instance = [[0, 1], [2, 3]]
    paths = [
        [{'S', 'E'}, {'S', 'W'}],   # row 0: (0,0)=S,E  (0,1)=S,W
        [{'N', 'E'}, {'N', 'W'}],   # row 1: (1,0)=N,E  (1,1)=N,W
    ]

    p = Problem('labyrinth_2x2')

    Card = UserType("Card")
    Direction = UserType("Direction")
    N_dir = Object("N_dir", Direction)
    S_dir = Object("S_dir", Direction)
    E_dir = Object("E_dir", Direction)
    W_dir = Object("W_dir", Direction)
    direction_by_name = {"N": N_dir, "S": S_dir, "E": E_dir, "W": W_dir}
    p.add_objects([N_dir, S_dir, E_dir, W_dir])
    p.add_objects([Object(f"card_{i}", Card) for i in range(n_cards)])
    card_0 = p.object('card_0')

    card_at = Fluent('card_at', ArrayType(n, ArrayType(n, Card)))
    p.add_fluent(card_at)  # no default — we initialize the whole array below

    robot_at = Fluent('robot_at', Card)
    p.add_fluent(robot_at, default_initial_value=card_0)

    connections = Fluent('connections', c=Card, d=Direction)
    p.add_fluent(connections, default_initial_value=False)

    # Set the whole 2D array at once — required so FluentExp(card_at) appears in
    # explicit_initial_values (per-cell ARRAY_READ keys are not recognized by
    # _initialize_fluents which checks arity-0 fluent expressions).
    initial_grid = [[p.object(f'card_{instance[r][c]}') for c in range(n)] for r in range(n)]
    p.set_initial_value(card_at, initial_grid)

    for r in range(n):
        for c in range(n):
            card_obj = p.object(f'card_{instance[r][c]}')
            for d in paths[r][c]:
                p.set_initial_value(connections(card_obj, direction_by_name[d]), True)

    p.set_initial_value(robot_at, p.object('card_0'))

    # Move actions
    move_north = InstantaneousAction('move_north', r=IntType(1, n - 1), c=IntType(0, n - 1))
    r, c = move_north.parameter('r'), move_north.parameter('c')
    move_north.add_precondition(Equals(robot_at, card_at[r][c]))
    move_north.add_precondition(connections(card_at[r][c], N_dir))
    move_north.add_precondition(connections(card_at[r - 1][c], S_dir))
    move_north.add_effect(robot_at, card_at[r - 1][c])
    p.add_action(move_north)

    move_south = InstantaneousAction('move_south', r=IntType(0, n - 2), c=IntType(0, n - 1))
    r, c = move_south.parameter('r'), move_south.parameter('c')
    move_south.add_precondition(Equals(robot_at, card_at[r][c]))
    move_south.add_precondition(connections(card_at[r][c], S_dir))
    move_south.add_precondition(connections(card_at[r + 1][c], N_dir))
    move_south.add_effect(robot_at, card_at[r + 1][c])
    p.add_action(move_south)

    move_east = InstantaneousAction('move_east', r=IntType(0, n - 1), c=IntType(0, n - 2))
    r, c = move_east.parameter('r'), move_east.parameter('c')
    move_east.add_precondition(Equals(robot_at, card_at[r][c]))
    move_east.add_precondition(connections(card_at[r][c], E_dir))
    move_east.add_precondition(connections(card_at[r][c + 1], W_dir))
    move_east.add_effect(robot_at, card_at[r][c + 1])
    p.add_action(move_east)

    move_west = InstantaneousAction('move_west', r=IntType(0, n - 1), c=IntType(1, n - 1))
    r, c = move_west.parameter('r'), move_west.parameter('c')
    move_west.add_precondition(Equals(robot_at, card_at[r][c]))
    move_west.add_precondition(connections(card_at[r][c], W_dir))
    move_west.add_precondition(connections(card_at[r][c - 1], E_dir))
    move_west.add_effect(robot_at, card_at[r][c - 1])
    p.add_action(move_west)

    # Goal: reach bottom-right cell (whichever card is there).
    p.add_goal(Equals(robot_at, card_at[n - 1][n - 1]))

    return p
