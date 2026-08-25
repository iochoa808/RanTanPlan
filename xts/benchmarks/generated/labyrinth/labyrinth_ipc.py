"""
Shared generator for the 40 real IPC 2023 Labyrinth benchmark instances
(p{n}_{shuffle}_{seed}, n in {4,5}, shuffle in {5,10,15,20}, seed in
{1..5} -- the actual paper benchmark, NOT the synthetic fully-open-grid
scaling sweep in xts/benchmarks/unit/_labyrinth_scaling.py).

Replicates the exact UP model in
~/unified-planning/docs/extensions/domains/labyrinth/Labyrinth.py
(generalized from its hardcoded n=4 example to arbitrary n): an nxn
ArrayType(Card) fluent tracking which shuffled card sits at each grid cell,
a connections(Card, Direction) predicate per card, 4 move actions (2
IntType(0,n-1) params each) and 4 row/column rotate actions (1
IntType(0,n-1) param each) using Forall/RangeVariable quantified
preconditions and effects -- this is what makes the domain a meaningful
test of RTP's native array + quantifier handling (arrays-of-usertype,
forall-effects), unlike the plain open-grid model in _labyrinth_scaling.py.

Instance data (initial card arrangement + per-card direction connections)
is loaded from up-format-instances/*.txt at generation time and embedded
below as literals -- see gen_labyrinth.py (one-off codegen script, not
committed) for the parsing step. One instance (p4_5_1) is only present
under the oddly-underscored filename "p_4_5_1.txt" in the source repo;
irrelevant here since the data is already embedded.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))

from unified_planning.shortcuts import *

Card = UserType("Card")
Direction = UserType("Direction")


def build_labyrinth(name: str, n: int, instance_grid, paths):
    n_cards = n * n
    p = Problem(name)

    N_dir = Object("N", Direction)
    S_dir = Object("S", Direction)
    E_dir = Object("E", Direction)
    W_dir = Object("W", Direction)
    direction_by_name = {"N": N_dir, "S": S_dir, "E": E_dir, "W": W_dir}
    p.add_objects([N_dir, S_dir, E_dir, W_dir])
    p.add_objects([Object(f"card_{i}", Card) for i in range(n_cards)])
    card_0 = p.object('card_0')

    card_at = Fluent('card_at', ArrayType(n, ArrayType(n, Card)))
    p.add_fluent(card_at, default_initial_value=card_0)

    robot_at = Fluent('robot_at', Card)
    p.add_fluent(robot_at, default_initial_value=card_0)
    # RTP's native ingestion requires an explicit :init entry for every
    # object fluent -- default_initial_value alone isn't enough for a plain
    # (unparameterized) object fluent, unlike card_at above which is fully
    # covered by the explicit whole-array assignment.
    p.set_initial_value(robot_at, card_0)

    connections = Fluent('connections', c=Card, d=Direction)
    p.add_fluent(connections, default_initial_value=False)

    # Whole-array assignment, not per-cell card_at[r][c] indexing: the
    # per-cell form (as in the original Labyrinth.py) hits an
    # "AssertionError: fluent field must be a fluent" in this UP version
    # (fluent_exp.is_fluent_exp() rejects ARRAY_SELECT expressions as an
    # assignment target) -- confirmed by running the original script
    # unmodified, so it's a pre-existing incompatibility, not new here.
    initial_grid = [
        [p.object(f'card_{instance_grid[r][c]}') for c in range(n)]
        for r in range(n)
    ]
    p.set_initial_value(card_at, initial_grid)

    for r in range(n):
        for c in range(n):
            card_object = p.object(f'card_{instance_grid[r][c]}')
            for d in paths[r][c]:
                p.set_initial_value(connections(card_object, direction_by_name[d]), True)

    move_north = InstantaneousAction('move_north', r=IntType(0, n - 1), c=IntType(0, n - 1))
    r, c = move_north.parameter('r'), move_north.parameter('c')
    move_north.add_precondition(Equals(robot_at, card_at[r][c]))
    move_north.add_precondition(connections(card_at[r][c], N_dir))
    move_north.add_precondition(connections(card_at[r - 1][c], S_dir))
    move_north.add_effect(robot_at, card_at[r - 1][c])
    p.add_action(move_north)

    move_south = InstantaneousAction('move_south', r=IntType(0, n - 1), c=IntType(0, n - 1))
    r, c = move_south.parameter('r'), move_south.parameter('c')
    move_south.add_precondition(Equals(robot_at, card_at[r][c]))
    move_south.add_precondition(connections(card_at[r][c], S_dir))
    move_south.add_precondition(connections(card_at[r + 1][c], N_dir))
    move_south.add_effect(robot_at, card_at[r + 1][c])
    p.add_action(move_south)

    move_east = InstantaneousAction('move_east', r=IntType(0, n - 1), c=IntType(0, n - 1))
    r, c = move_east.parameter('r'), move_east.parameter('c')
    move_east.add_precondition(Equals(robot_at, card_at[r][c]))
    move_east.add_precondition(connections(card_at[r][c], E_dir))
    move_east.add_precondition(connections(card_at[r][c + 1], W_dir))
    move_east.add_effect(robot_at, card_at[r][c + 1])
    p.add_action(move_east)

    move_west = InstantaneousAction('move_west', r=IntType(0, n - 1), c=IntType(0, n - 1))
    r, c = move_west.parameter('r'), move_west.parameter('c')
    move_west.add_precondition(Equals(robot_at, card_at[r][c]))
    move_west.add_precondition(connections(card_at[r][c], W_dir))
    move_west.add_precondition(connections(card_at[r][c - 1], E_dir))
    move_west.add_effect(robot_at, card_at[r][c - 1])
    p.add_action(move_west)

    rotate_col_up = InstantaneousAction('rotate_col_up', c=IntType(0, n - 1))
    c = rotate_col_up.parameter('c')
    all_rows = RangeVariable('all_rows', 0, n - 1)
    rotate_col_up.add_precondition(Forall(Not(Equals(robot_at, card_at[all_rows][c])), all_rows))
    rotated_rows = RangeVariable("rotated_rows", 1, n - 1)
    rotate_col_up.add_effect(card_at[rotated_rows - 1][c], card_at[rotated_rows][c], forall=[rotated_rows])
    rotate_col_up.add_effect(card_at[n - 1][c], card_at[0][c])
    p.add_action(rotate_col_up)

    rotate_col_down = InstantaneousAction('rotate_col_down', c=IntType(0, n - 1))
    c = rotate_col_down.parameter('c')
    all_rows = RangeVariable("all_rows", 0, n - 1)
    rotate_col_down.add_precondition(Forall(Not(Equals(robot_at, card_at[all_rows][c])), all_rows))
    rotated_rows = RangeVariable("rotated_rows", 1, n - 1)
    rotate_col_down.add_effect(card_at[rotated_rows][c], card_at[rotated_rows - 1][c], forall=[rotated_rows])
    rotate_col_down.add_effect(card_at[0][c], card_at[n - 1][c])
    p.add_action(rotate_col_down)

    rotate_row_left = InstantaneousAction('rotate_row_left', r=IntType(0, n - 1))
    r = rotate_row_left.parameter('r')
    all_cols = RangeVariable("all_cols", 0, n - 1)
    rotate_row_left.add_precondition(Forall(Not(Equals(robot_at, card_at[r][all_cols])), all_cols))
    rotated_cols = RangeVariable("rotated_cols", 0, n - 2)
    rotate_row_left.add_effect(card_at[r][rotated_cols], card_at[r][rotated_cols + 1], forall=[rotated_cols])
    rotate_row_left.add_effect(card_at[r][n - 1], card_at[r][0])
    p.add_action(rotate_row_left)

    rotate_row_right = InstantaneousAction('rotate_row_right', r=IntType(0, n - 1))
    r = rotate_row_right.parameter('r')
    all_cols = RangeVariable("all_cols", 0, n - 1)
    rotate_row_right.add_precondition(Forall(Not(Equals(robot_at, card_at[r][all_cols])), all_cols))
    rotated_cols = RangeVariable("rotated_cols", 1, n - 1)
    rotate_row_right.add_effect(card_at[r][rotated_cols], card_at[r][rotated_cols - 1], forall=[rotated_cols])
    rotate_row_right.add_effect(card_at[r][0], card_at[r][n - 1])
    p.add_action(rotate_row_right)

    p.add_goal(Equals(robot_at, card_at[n - 1][n - 1]))
    p.add_goal(connections(card_at[n - 1][n - 1], S_dir))

    costs = {
        move_north: Int(1), move_south: Int(1), move_east: Int(1), move_west: Int(1),
        rotate_col_up: Int(1), rotate_col_down: Int(1),
        rotate_row_left: Int(1), rotate_row_right: Int(1),
    }
    p.add_quality_metric(MinimizeActionCosts(costs))
    return p


INSTANCES = {
    'p4_5_2': (4, [[0, 1, 14, 3], [6, 7, 4, 2], [10, 5, 8, 9], [12, 13, 11, 15]], [[{'E', 'W'}, {'E', 'N', 'W'}, {'S', 'E', 'N'}, {'S', 'N'}], [{'E', 'N'}, {'S', 'E', 'W'}, {'S', 'W'}, {'S', 'N', 'W'}], [{'S', 'E', 'N'}, {'S', 'E'}, {'S', 'N', 'W'}, {'N', 'W'}], [{'S', 'E', 'N'}, {'S', 'W'}, {'E', 'N', 'W'}, {'S', 'E', 'W'}]]),
    'p4_15_3': (4, [[0, 1, 5, 3], [7, 11, 8, 14], [10, 13, 9, 6], [12, 2, 4, 15]], [[{'E', 'W'}, {'S', 'E', 'N', 'W'}, {'S', 'E', 'N'}, {'S', 'N', 'W'}], [{'S', 'E', 'N'}, {'S', 'E', 'N', 'W'}, {'N', 'W'}, {'E', 'N', 'W'}], [{'S', 'E'}, {'E', 'N', 'W'}, {'S', 'W'}, {'N', 'W'}], [{'S', 'W'}, {'S', 'E'}, {'S', 'N', 'W'}, {'S', 'W'}]]),
    'p4_5_3': (4, [[0, 1, 14, 3], [4, 5, 2, 7], [10, 11, 8, 6], [12, 13, 9, 15]], [[{'E', 'W'}, {'S', 'E', 'N', 'W'}, {'E', 'N', 'W'}, {'S', 'N', 'W'}], [{'S', 'N', 'W'}, {'S', 'E', 'N'}, {'S', 'E'}, {'S', 'E', 'N'}], [{'S', 'E'}, {'S', 'E', 'N', 'W'}, {'N', 'W'}, {'N', 'W'}], [{'S', 'W'}, {'E', 'N', 'W'}, {'S', 'W'}, {'S', 'W'}]]),
    'p4_10_3': (4, [[0, 13, 9, 3], [14, 2, 1, 4], [10, 7, 5, 6], [12, 11, 8, 15]], [[{'E', 'W'}, {'E', 'N', 'W'}, {'S', 'W'}, {'S', 'N', 'W'}], [{'E', 'N', 'W'}, {'S', 'E'}, {'S', 'E', 'N', 'W'}, {'S', 'N', 'W'}], [{'S', 'E'}, {'S', 'E', 'N'}, {'S', 'E', 'N'}, {'N', 'W'}], [{'S', 'W'}, {'S', 'E', 'N', 'W'}, {'N', 'W'}, {'S', 'W'}]]),
    'p4_15_2': (4, [[0, 13, 6, 3], [5, 1, 11, 14], [7, 2, 9, 10], [12, 4, 8, 15]], [[{'E', 'W'}, {'S', 'W'}, {'E', 'N'}, {'S', 'N'}], [{'S', 'E'}, {'E', 'N', 'W'}, {'E', 'N', 'W'}, {'S', 'E', 'N'}], [{'S', 'E', 'W'}, {'S', 'N', 'W'}, {'N', 'W'}, {'S', 'E', 'N'}], [{'S', 'E', 'N'}, {'S', 'W'}, {'S', 'N', 'W'}, {'S', 'E', 'W'}]]),
    'p4_15_1': (4, [[0, 6, 1, 3], [9, 7, 14, 4], [10, 8, 2, 13], [12, 5, 11, 15]], [[{'E', 'N'}, {'S', 'N', 'W'}, {'E', 'W'}, {'S', 'E', 'N'}], [{'S', 'E', 'N', 'W'}, {'S', 'W'}, {'E', 'N'}, {'S', 'W'}], [{'S', 'W'}, {'N', 'W'}, {'S', 'E', 'W'}, {'E', 'N'}], [{'S', 'N'}, {'S', 'E'}, {'E', 'W'}, {'S', 'N', 'W'}]]),
    'p4_5_4': (4, [[0, 6, 7, 3], [5, 9, 10, 4], [8, 13, 14, 11], [12, 1, 2, 15]], [[{'E', 'W'}, {'S', 'W'}, {'S', 'E', 'N', 'W'}, {'E', 'N', 'W'}], [{'E', 'N', 'W'}, {'S', 'W'}, {'S', 'N'}, {'S', 'E'}], [{'S', 'E', 'N'}, {'S', 'W'}, {'S', 'E', 'N'}, {'E', 'W'}], [{'N', 'W'}, {'S', 'N', 'W'}, {'S', 'N'}, {'S', 'W'}]]),
    'p4_20_2': (4, [[0, 2, 8, 3], [4, 6, 5, 1], [7, 13, 14, 10], [12, 11, 9, 15]], [[{'E', 'W'}, {'S', 'N', 'W'}, {'S', 'N', 'W'}, {'S', 'N'}], [{'S', 'W'}, {'E', 'N'}, {'S', 'E'}, {'E', 'N', 'W'}], [{'S', 'E', 'W'}, {'S', 'W'}, {'S', 'E', 'N'}, {'S', 'E', 'N'}], [{'S', 'E', 'N'}, {'E', 'N', 'W'}, {'N', 'W'}, {'S', 'E', 'W'}]]),
    'p4_20_1': (4, [[0, 6, 11, 3], [9, 7, 14, 1], [10, 8, 4, 13], [12, 5, 2, 15]], [[{'E', 'N'}, {'S', 'N', 'W'}, {'E', 'W'}, {'S', 'E', 'N'}], [{'S', 'E', 'N', 'W'}, {'S', 'W'}, {'E', 'N'}, {'E', 'W'}], [{'S', 'W'}, {'N', 'W'}, {'S', 'W'}, {'E', 'N'}], [{'S', 'N'}, {'S', 'E'}, {'S', 'E', 'W'}, {'S', 'N', 'W'}]]),
    'p4_20_4': (4, [[0, 1, 7, 3], [5, 6, 8, 4], [9, 13, 14, 10], [12, 11, 2, 15]], [[{'E', 'W'}, {'S', 'N', 'W'}, {'S', 'E', 'N', 'W'}, {'E', 'N', 'W'}], [{'E', 'N', 'W'}, {'S', 'W'}, {'S', 'E', 'N'}, {'S', 'E'}], [{'S', 'W'}, {'S', 'W'}, {'S', 'E', 'N'}, {'S', 'N'}], [{'N', 'W'}, {'E', 'W'}, {'S', 'N'}, {'S', 'W'}]]),
    'p4_15_4': (4, [[0, 1, 2, 3], [5, 6, 7, 4], [8, 10, 13, 11], [12, 9, 14, 15]], [[{'E', 'W'}, {'S', 'N', 'W'}, {'S', 'N'}, {'E', 'N', 'W'}], [{'E', 'N', 'W'}, {'S', 'W'}, {'S', 'E', 'N', 'W'}, {'S', 'E'}], [{'S', 'E', 'N'}, {'S', 'N'}, {'S', 'W'}, {'E', 'W'}], [{'N', 'W'}, {'S', 'W'}, {'S', 'E', 'N'}, {'S', 'W'}]]),
    'p4_10_2': (4, [[0, 1, 8, 3], [5, 7, 11, 6], [10, 4, 14, 9], [12, 13, 2, 15]], [[{'E', 'W'}, {'E', 'N', 'W'}, {'S', 'N', 'W'}, {'S', 'N'}], [{'S', 'E'}, {'S', 'E', 'W'}, {'E', 'N', 'W'}, {'E', 'N'}], [{'S', 'E', 'N'}, {'S', 'W'}, {'S', 'E', 'N'}, {'N', 'W'}], [{'S', 'E', 'N'}, {'S', 'W'}, {'S', 'N', 'W'}, {'S', 'E', 'W'}]]),
    'p4_10_4': (4, [[0, 10, 7, 3], [5, 9, 13, 4], [8, 1, 14, 11], [12, 6, 2, 15]], [[{'E', 'W'}, {'S', 'N'}, {'S', 'E', 'N', 'W'}, {'E', 'N', 'W'}], [{'E', 'N', 'W'}, {'S', 'W'}, {'S', 'W'}, {'S', 'E'}], [{'S', 'E', 'N'}, {'S', 'N', 'W'}, {'S', 'E', 'N'}, {'E', 'W'}], [{'N', 'W'}, {'S', 'W'}, {'S', 'N'}, {'S', 'W'}]]),
    'p4_5_5': (4, [[0, 9, 14, 3], [7, 4, 2, 6], [5, 1, 13, 8], [12, 10, 11, 15]], [[{'S', 'W'}, {'E', 'W'}, {'E', 'N'}, {'S', 'E', 'N', 'W'}], [{'S', 'N'}, {'S', 'N'}, {'S', 'E', 'W'}, {'S', 'W'}], [{'E', 'N'}, {'S', 'E'}, {'S', 'E', 'N'}, {'E', 'N'}], [{'N', 'W'}, {'S', 'N', 'W'}, {'S', 'N'}, {'S', 'N'}]]),
    'p4_5_1': (4, [[0, 1, 2, 3], [9, 5, 7, 4], [10, 6, 8, 13], [12, 11, 14, 15]], [[{'E', 'N'}, {'E', 'W'}, {'S', 'E', 'W'}, {'S', 'E', 'N'}], [{'S', 'E', 'N', 'W'}, {'S', 'E'}, {'S', 'W'}, {'S', 'W'}], [{'S', 'W'}, {'S', 'N', 'W'}, {'N', 'W'}, {'E', 'N'}], [{'S', 'N'}, {'E', 'W'}, {'E', 'N'}, {'S', 'N', 'W'}]]),
    'p4_10_5': (4, [[0, 1, 14, 3], [7, 9, 10, 6], [4, 2, 8, 5], [12, 13, 11, 15]], [[{'S', 'W'}, {'S', 'E'}, {'E', 'N'}, {'S', 'E', 'N', 'W'}], [{'S', 'N'}, {'E', 'W'}, {'S', 'N', 'W'}, {'S', 'W'}], [{'S', 'N'}, {'S', 'E', 'W'}, {'E', 'N'}, {'E', 'N'}], [{'N', 'W'}, {'S', 'E', 'N'}, {'S', 'N'}, {'S', 'N'}]]),
    'p4_10_1': (4, [[0, 8, 2, 3], [9, 5, 11, 4], [6, 1, 13, 10], [12, 7, 14, 15]], [[{'E', 'N'}, {'N', 'W'}, {'S', 'E', 'W'}, {'S', 'E', 'N'}], [{'S', 'E', 'N', 'W'}, {'S', 'E'}, {'E', 'W'}, {'S', 'W'}], [{'S', 'N', 'W'}, {'E', 'W'}, {'E', 'N'}, {'S', 'W'}], [{'S', 'N'}, {'S', 'W'}, {'E', 'N'}, {'S', 'N', 'W'}]]),
    'p4_20_3': (4, [[0, 7, 4, 3], [5, 14, 2, 13], [10, 1, 8, 6], [12, 11, 9, 15]], [[{'E', 'W'}, {'S', 'E', 'N'}, {'S', 'N', 'W'}, {'S', 'N', 'W'}], [{'S', 'E', 'N'}, {'E', 'N', 'W'}, {'S', 'E'}, {'E', 'N', 'W'}], [{'S', 'E'}, {'S', 'E', 'N', 'W'}, {'N', 'W'}, {'N', 'W'}], [{'S', 'W'}, {'S', 'E', 'N', 'W'}, {'S', 'W'}, {'S', 'W'}]]),
    'p4_20_5': (4, [[0, 10, 9, 3], [8, 13, 7, 11], [4, 1, 2, 5], [12, 6, 14, 15]], [[{'S', 'W'}, {'S', 'N', 'W'}, {'E', 'W'}, {'S', 'E', 'N', 'W'}], [{'E', 'N'}, {'S', 'E', 'N'}, {'S', 'N'}, {'S', 'N'}], [{'S', 'N'}, {'S', 'E'}, {'S', 'E', 'W'}, {'E', 'N'}], [{'N', 'W'}, {'S', 'W'}, {'E', 'N'}, {'S', 'N'}]]),
    'p5_5_1': (5, [[0, 1, 2, 7, 4], [9, 5, 6, 12, 8], [10, 11, 19, 13, 14], [16, 17, 18, 23, 15], [20, 21, 22, 3, 24]], [[{'S', 'E', 'N'}, {'S', 'W'}, {'E', 'W'}, {'S', 'E', 'N', 'W'}, {'S', 'N', 'W'}], [{'S', 'W'}, {'E', 'N'}, {'E', 'N', 'W'}, {'S', 'N', 'W'}, {'E', 'W'}], [{'S', 'E'}, {'S', 'E'}, {'N', 'W'}, {'S', 'W'}, {'S', 'N'}], [{'S', 'W'}, {'N', 'W'}, {'S', 'E', 'N'}, {'E', 'N', 'W'}, {'S', 'N'}], [{'S', 'E', 'N'}, {'S', 'E', 'N'}, {'E', 'W'}, {'S', 'W'}, {'S', 'W'}]]),
    'p4_15_5': (4, [[0, 6, 7, 3], [10, 2, 11, 8], [4, 13, 14, 5], [12, 1, 9, 15]], [[{'S', 'W'}, {'S', 'W'}, {'S', 'N'}, {'S', 'E', 'N', 'W'}], [{'S', 'N', 'W'}, {'S', 'E', 'W'}, {'S', 'N'}, {'E', 'N'}], [{'S', 'N'}, {'S', 'E', 'N'}, {'E', 'N'}, {'E', 'N'}], [{'N', 'W'}, {'S', 'E'}, {'E', 'W'}, {'S', 'N'}]]),
    'p5_5_2': (5, [[0, 21, 2, 23, 4], [1, 7, 8, 3, 5], [10, 6, 12, 9, 14], [18, 19, 15, 11, 13], [20, 16, 22, 17, 24]], [[{'S', 'E'}, {'S', 'N'}, {'S', 'E', 'W'}, {'E', 'N', 'W'}, {'S', 'W'}], [{'E', 'N', 'W'}, {'S', 'N', 'W'}, {'S', 'E'}, {'S', 'E', 'W'}, {'N', 'W'}], [{'S', 'N', 'W'}, {'S', 'E', 'W'}, {'S', 'N'}, {'S', 'N'}, {'S', 'E', 'N', 'W'}], [{'S', 'E'}, {'S', 'N', 'W'}, {'S', 'E', 'N', 'W'}, {'E', 'W'}, {'S', 'N', 'W'}], [{'S', 'N'}, {'E', 'W'}, {'N', 'W'}, {'S', 'E'}, {'S', 'N'}]]),
    'p5_5_3': (5, [[0, 21, 22, 7, 4], [9, 1, 2, 13, 8], [10, 5, 6, 19, 14], [11, 12, 18, 23, 15], [20, 16, 17, 3, 24]], [[{'E', 'W'}, {'S', 'W'}, {'E', 'W'}, {'S', 'W'}, {'N', 'W'}], [{'S', 'W'}, {'S', 'E', 'W'}, {'N', 'W'}, {'S', 'E', 'W'}, {'E', 'N'}], [{'N', 'W'}, {'S', 'N', 'W'}, {'E', 'N'}, {'S', 'N', 'W'}, {'S', 'N', 'W'}], [{'N', 'W'}, {'S', 'E', 'N'}, {'S', 'E', 'W'}, {'S', 'W'}, {'S', 'W'}], [{'E', 'N', 'W'}, {'S', 'N'}, {'E', 'W'}, {'S', 'N', 'W'}, {'S', 'N'}]]),
    'p5_5_4': (5, [[0, 6, 7, 8, 4], [5, 11, 12, 13, 9], [10, 16, 17, 18, 14], [23, 19, 15, 21, 22], [20, 1, 2, 3, 24]], [[{'E', 'W'}, {'S', 'N'}, {'S', 'N', 'W'}, {'S', 'E', 'N', 'W'}, {'S', 'N'}], [{'S', 'N'}, {'S', 'E', 'N', 'W'}, {'S', 'N', 'W'}, {'E', 'W'}, {'S', 'W'}], [{'E', 'W'}, {'S', 'W'}, {'E', 'N'}, {'S', 'W'}, {'S', 'W'}], [{'E', 'N'}, {'E', 'N'}, {'N', 'W'}, {'S', 'W'}, {'S', 'N'}], [{'S', 'N'}, {'E', 'W'}, {'S', 'W'}, {'E', 'N', 'W'}, {'S', 'W'}]]),
    'p5_5_5': (5, [[0, 6, 2, 3, 4], [11, 7, 8, 9, 5], [15, 12, 13, 14, 10], [18, 19, 21, 16, 17], [20, 1, 22, 23, 24]], [[{'S', 'W'}, {'S', 'W'}, {'S', 'W'}, {'S', 'E'}, {'S', 'N'}], [{'S', 'W'}, {'E', 'W'}, {'S', 'N'}, {'S', 'W'}, {'S', 'N'}], [{'N', 'W'}, {'S', 'W'}, {'S', 'N'}, {'S', 'E', 'W'}, {'E', 'N'}], [{'S', 'E', 'W'}, {'S', 'W'}, {'N', 'W'}, {'S', 'E', 'N', 'W'}, {'E', 'W'}], [{'S', 'E'}, {'S', 'W'}, {'E', 'N'}, {'S', 'N', 'W'}, {'S', 'N', 'W'}]]),
    'p5_10_1': (5, [[0, 6, 2, 3, 4], [5, 10, 7, 8, 9], [14, 18, 11, 19, 12], [17, 21, 13, 15, 16], [20, 1, 22, 23, 24]], [[{'S', 'E', 'N'}, {'E', 'N', 'W'}, {'E', 'W'}, {'S', 'W'}, {'S', 'N', 'W'}], [{'E', 'N'}, {'S', 'E'}, {'S', 'E', 'N', 'W'}, {'E', 'W'}, {'S', 'W'}], [{'S', 'N'}, {'S', 'E', 'N'}, {'S', 'E'}, {'N', 'W'}, {'S', 'N', 'W'}], [{'N', 'W'}, {'S', 'E', 'N'}, {'S', 'W'}, {'S', 'N'}, {'S', 'W'}], [{'S', 'E', 'N'}, {'S', 'W'}, {'E', 'W'}, {'E', 'N', 'W'}, {'S', 'W'}]]),
    'p5_10_2': (5, [[0, 21, 2, 23, 4], [1, 7, 8, 6, 5], [9, 14, 10, 13, 3], [19, 15, 12, 11, 18], [20, 16, 22, 17, 24]], [[{'S', 'E'}, {'S', 'N'}, {'S', 'E', 'W'}, {'E', 'N', 'W'}, {'S', 'W'}], [{'E', 'N', 'W'}, {'S', 'N', 'W'}, {'S', 'E'}, {'S', 'E', 'W'}, {'N', 'W'}], [{'S', 'N'}, {'S', 'E', 'N', 'W'}, {'S', 'N', 'W'}, {'S', 'N', 'W'}, {'S', 'E', 'W'}], [{'S', 'N', 'W'}, {'S', 'E', 'N', 'W'}, {'S', 'N'}, {'E', 'W'}, {'S', 'E'}], [{'S', 'N'}, {'E', 'W'}, {'N', 'W'}, {'S', 'E'}, {'S', 'N'}]]),
    'p5_10_3': (5, [[0, 21, 2, 19, 4], [8, 9, 1, 6, 15], [10, 5, 23, 3, 14], [12, 18, 17, 7, 11], [20, 16, 22, 13, 24]], [[{'E', 'W'}, {'S', 'W'}, {'N', 'W'}, {'S', 'N', 'W'}, {'N', 'W'}], [{'E', 'N'}, {'S', 'W'}, {'S', 'E', 'W'}, {'E', 'N'}, {'S', 'W'}], [{'N', 'W'}, {'S', 'N', 'W'}, {'S', 'W'}, {'S', 'N', 'W'}, {'S', 'N', 'W'}], [{'S', 'E', 'N'}, {'S', 'E', 'W'}, {'E', 'W'}, {'S', 'W'}, {'N', 'W'}], [{'E', 'N', 'W'}, {'S', 'N'}, {'E', 'W'}, {'S', 'E', 'W'}, {'S', 'N'}]]),
    'p5_10_4': (5, [[0, 6, 7, 3, 4], [5, 11, 12, 8, 9], [13, 14, 10, 16, 17], [23, 19, 18, 21, 22], [20, 1, 2, 15, 24]], [[{'E', 'W'}, {'S', 'N'}, {'S', 'N', 'W'}, {'E', 'N', 'W'}, {'S', 'N'}], [{'S', 'N'}, {'S', 'E', 'N', 'W'}, {'S', 'N', 'W'}, {'S', 'E', 'N', 'W'}, {'S', 'W'}], [{'E', 'W'}, {'S', 'W'}, {'E', 'W'}, {'S', 'W'}, {'E', 'N'}], [{'E', 'N'}, {'E', 'N'}, {'S', 'W'}, {'S', 'W'}, {'S', 'N'}], [{'S', 'N'}, {'E', 'W'}, {'S', 'W'}, {'N', 'W'}, {'S', 'W'}]]),
    'p5_10_5': (5, [[0, 6, 22, 14, 4], [7, 2, 3, 16, 11], [15, 12, 8, 23, 10], [18, 19, 13, 5, 17], [20, 1, 21, 9, 24]], [[{'S', 'W'}, {'S', 'W'}, {'E', 'N'}, {'S', 'E', 'W'}, {'S', 'N'}], [{'E', 'W'}, {'S', 'W'}, {'S', 'E'}, {'S', 'E', 'N', 'W'}, {'S', 'W'}], [{'N', 'W'}, {'S', 'W'}, {'S', 'N'}, {'S', 'N', 'W'}, {'E', 'N'}], [{'S', 'E', 'W'}, {'S', 'W'}, {'S', 'N'}, {'S', 'N'}, {'E', 'W'}], [{'S', 'E'}, {'S', 'W'}, {'N', 'W'}, {'S', 'W'}, {'S', 'N', 'W'}]]),
    'p5_15_1': (5, [[0, 6, 22, 23, 4], [9, 5, 2, 7, 3], [14, 18, 10, 8, 12], [17, 11, 13, 19, 16], [20, 1, 21, 15, 24]], [[{'S', 'E', 'N'}, {'E', 'N', 'W'}, {'E', 'W'}, {'E', 'N', 'W'}, {'S', 'N', 'W'}], [{'S', 'W'}, {'E', 'N'}, {'E', 'W'}, {'S', 'E', 'N', 'W'}, {'S', 'W'}], [{'S', 'N'}, {'S', 'E', 'N'}, {'S', 'E'}, {'E', 'W'}, {'S', 'N', 'W'}], [{'N', 'W'}, {'S', 'E'}, {'S', 'W'}, {'N', 'W'}, {'S', 'W'}], [{'S', 'E', 'N'}, {'S', 'W'}, {'S', 'E', 'N'}, {'S', 'N'}, {'S', 'W'}]]),
    'p5_15_2': (5, [[0, 21, 8, 17, 4], [1, 7, 14, 23, 5], [3, 9, 11, 6, 13], [12, 22, 10, 19, 15], [20, 16, 2, 18, 24]], [[{'S', 'E'}, {'S', 'N'}, {'S', 'E'}, {'S', 'E'}, {'S', 'W'}], [{'E', 'N', 'W'}, {'S', 'N', 'W'}, {'S', 'E', 'N', 'W'}, {'E', 'N', 'W'}, {'N', 'W'}], [{'S', 'E', 'W'}, {'S', 'N'}, {'E', 'W'}, {'S', 'E', 'W'}, {'S', 'N', 'W'}], [{'S', 'N'}, {'N', 'W'}, {'S', 'N', 'W'}, {'S', 'N', 'W'}, {'S', 'E', 'N', 'W'}], [{'S', 'N'}, {'E', 'W'}, {'S', 'E', 'W'}, {'S', 'E'}, {'S', 'N'}]]),
    'p5_15_4': (5, [[0, 1, 2, 15, 4], [5, 6, 7, 3, 9], [10, 8, 12, 13, 11], [23, 14, 17, 16, 22], [20, 19, 18, 21, 24]], [[{'E', 'W'}, {'E', 'W'}, {'S', 'W'}, {'N', 'W'}, {'S', 'N'}], [{'S', 'N'}, {'S', 'N'}, {'S', 'N', 'W'}, {'E', 'N', 'W'}, {'S', 'W'}], [{'E', 'W'}, {'S', 'E', 'N', 'W'}, {'S', 'N', 'W'}, {'E', 'W'}, {'S', 'E', 'N', 'W'}], [{'E', 'N'}, {'S', 'W'}, {'E', 'N'}, {'S', 'W'}, {'S', 'N'}], [{'S', 'N'}, {'E', 'N'}, {'S', 'W'}, {'S', 'W'}, {'S', 'W'}]]),
    'p5_15_5': (5, [[0, 2, 22, 16, 4], [7, 12, 3, 10, 11], [15, 18, 8, 23, 5], [17, 1, 19, 13, 9], [20, 6, 21, 14, 24]], [[{'S', 'W'}, {'S', 'W'}, {'E', 'N'}, {'S', 'E', 'N', 'W'}, {'S', 'N'}], [{'E', 'W'}, {'S', 'W'}, {'S', 'E'}, {'E', 'N'}, {'S', 'W'}], [{'N', 'W'}, {'S', 'E', 'W'}, {'S', 'N'}, {'S', 'N', 'W'}, {'S', 'N'}], [{'E', 'W'}, {'S', 'W'}, {'S', 'W'}, {'S', 'N'}, {'S', 'W'}], [{'S', 'E'}, {'S', 'W'}, {'N', 'W'}, {'S', 'E', 'W'}, {'S', 'N', 'W'}]]),
    'p5_20_2': (5, [[0, 21, 8, 18, 4], [1, 7, 14, 17, 5], [3, 9, 11, 6, 23], [12, 22, 10, 13, 15], [20, 16, 2, 19, 24]], [[{'S', 'E'}, {'S', 'N'}, {'S', 'E'}, {'S', 'E'}, {'S', 'W'}], [{'E', 'N', 'W'}, {'S', 'N', 'W'}, {'S', 'E', 'N', 'W'}, {'S', 'E'}, {'N', 'W'}], [{'S', 'E', 'W'}, {'S', 'N'}, {'E', 'W'}, {'S', 'E', 'W'}, {'E', 'N', 'W'}], [{'S', 'N'}, {'N', 'W'}, {'S', 'N', 'W'}, {'S', 'N', 'W'}, {'S', 'E', 'N', 'W'}], [{'S', 'N'}, {'E', 'W'}, {'S', 'E', 'W'}, {'S', 'N', 'W'}, {'S', 'N'}]]),
    'p5_20_1': (5, [[0, 2, 22, 15, 4], [18, 7, 3, 23, 5], [14, 11, 10, 9, 12], [16, 17, 1, 8, 19], [20, 6, 21, 13, 24]], [[{'S', 'E', 'N'}, {'E', 'W'}, {'E', 'W'}, {'S', 'N'}, {'S', 'N', 'W'}], [{'S', 'E', 'N'}, {'S', 'E', 'N', 'W'}, {'S', 'W'}, {'E', 'N', 'W'}, {'E', 'N'}], [{'S', 'N'}, {'S', 'E'}, {'S', 'E'}, {'S', 'W'}, {'S', 'N', 'W'}], [{'S', 'W'}, {'N', 'W'}, {'S', 'W'}, {'E', 'W'}, {'N', 'W'}], [{'S', 'E', 'N'}, {'E', 'N', 'W'}, {'S', 'E', 'N'}, {'S', 'W'}, {'S', 'W'}]]),
    'p5_15_3': (5, [[0, 1, 7, 3, 4], [9, 5, 23, 15, 8], [10, 18, 17, 13, 14], [12, 16, 22, 19, 11], [20, 21, 2, 6, 24]], [[{'E', 'W'}, {'S', 'E', 'W'}, {'S', 'W'}, {'S', 'N', 'W'}, {'N', 'W'}], [{'S', 'W'}, {'S', 'N', 'W'}, {'S', 'W'}, {'S', 'W'}, {'E', 'N'}], [{'N', 'W'}, {'S', 'E', 'W'}, {'E', 'W'}, {'S', 'E', 'W'}, {'S', 'N', 'W'}], [{'S', 'E', 'N'}, {'S', 'N'}, {'E', 'W'}, {'S', 'N', 'W'}, {'N', 'W'}], [{'E', 'N', 'W'}, {'S', 'W'}, {'N', 'W'}, {'E', 'N'}, {'S', 'N'}]]),
    'p5_20_3': (5, [[0, 21, 7, 15, 4], [9, 1, 23, 13, 8], [10, 5, 17, 19, 14], [16, 18, 6, 11, 12], [20, 22, 2, 3, 24]], [[{'E', 'W'}, {'S', 'W'}, {'S', 'W'}, {'S', 'W'}, {'N', 'W'}], [{'S', 'W'}, {'S', 'E', 'W'}, {'S', 'W'}, {'S', 'E', 'W'}, {'E', 'N'}], [{'N', 'W'}, {'S', 'N', 'W'}, {'E', 'W'}, {'S', 'N', 'W'}, {'S', 'N', 'W'}], [{'S', 'N'}, {'S', 'E', 'W'}, {'E', 'N'}, {'N', 'W'}, {'S', 'E', 'N'}], [{'E', 'N', 'W'}, {'E', 'W'}, {'N', 'W'}, {'S', 'N', 'W'}, {'S', 'N'}]]),
    'p5_20_4': (5, [[0, 1, 2, 15, 4], [5, 6, 7, 3, 9], [10, 8, 12, 13, 11], [14, 16, 18, 22, 23], [20, 19, 17, 21, 24]], [[{'E', 'W'}, {'E', 'W'}, {'S', 'W'}, {'N', 'W'}, {'S', 'N'}], [{'S', 'N'}, {'S', 'N'}, {'S', 'N', 'W'}, {'E', 'N', 'W'}, {'S', 'W'}], [{'E', 'W'}, {'S', 'E', 'N', 'W'}, {'S', 'N', 'W'}, {'E', 'W'}, {'S', 'E', 'N', 'W'}], [{'S', 'W'}, {'S', 'W'}, {'S', 'W'}, {'S', 'N'}, {'E', 'N'}], [{'S', 'N'}, {'E', 'N'}, {'E', 'N'}, {'S', 'W'}, {'S', 'W'}]]),
    'p5_20_5': (5, [[0, 2, 22, 16, 4], [10, 12, 7, 18, 3], [1, 11, 23, 5, 15], [17, 8, 19, 13, 9], [20, 6, 21, 14, 24]], [[{'S', 'W'}, {'S', 'W'}, {'E', 'N'}, {'S', 'E', 'N', 'W'}, {'S', 'N'}], [{'E', 'N'}, {'S', 'W'}, {'E', 'W'}, {'S', 'E', 'W'}, {'S', 'E'}], [{'S', 'W'}, {'S', 'W'}, {'S', 'N', 'W'}, {'S', 'N'}, {'N', 'W'}], [{'E', 'W'}, {'S', 'N'}, {'S', 'W'}, {'S', 'N'}, {'S', 'W'}], [{'S', 'E'}, {'S', 'W'}, {'N', 'W'}, {'S', 'E', 'W'}, {'S', 'N', 'W'}]])
}
