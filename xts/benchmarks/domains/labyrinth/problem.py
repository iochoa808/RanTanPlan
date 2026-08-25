"""
Labyrinth — v1, 4×4 grid of card objects, instance i0.

card_at: ArrayType(4, ArrayType(4, Card))
robot_at: Card fluent
connections(card, direction): boolean predicate
Rotation actions shift an entire row or column cyclically.
PDDL-XTS counterpart: PDDL-XTS/labyrinth/instances/i0.pddl
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('labyrinth_i0')

    Card      = UserType('Card')
    Direction = UserType('Direction')
    N_dir = Object('N', Direction); S_dir = Object('S', Direction)
    E_dir = Object('E', Direction); W_dir = Object('W', Direction)
    p.add_objects([N_dir, S_dir, E_dir, W_dir])

    cards = [Object(f'card_{i}', Card) for i in range(16)]
    p.add_objects(cards)
    c = {i: cards[i] for i in range(16)}

    idx_t = IntType(0, 3)
    card_at   = Fluent('card_at', ArrayType(4, ArrayType(4, Card)))
    robot_at  = Fluent('robot_at', Card)
    connects  = Fluent('connections', BoolType(), card=Card, d=Direction)
    p.add_fluent(card_at)
    p.add_fluent(robot_at,  default_initial_value=c[0])
    p.set_initial_value(robot_at, c[0])
    p.add_fluent(connects,  default_initial_value=False)

    # Initial grid layout (from i0.pddl)
    p.set_initial_value(card_at, [
        [c[0],  c[6],  c[7],  c[3] ],
        [c[5],  c[9],  c[10], c[4] ],
        [c[8],  c[13], c[14], c[11]],
        [c[12], c[1],  c[2],  c[15]],
    ])

    # Open directions per card (from i0.pddl)
    card_dirs = {
        c[0]:  [W_dir, E_dir],
        c[6]:  [S_dir, W_dir],
        c[7]:  [N_dir, W_dir, S_dir, E_dir],
        c[3]:  [N_dir, W_dir, E_dir],
        c[5]:  [N_dir, W_dir, E_dir],
        c[9]:  [S_dir, W_dir],
        c[10]: [N_dir, S_dir],
        c[4]:  [S_dir, E_dir],
        c[8]:  [N_dir, S_dir, E_dir],
        c[13]: [S_dir, W_dir],
        c[14]: [N_dir, S_dir, E_dir],
        c[11]: [S_dir, E_dir],
        c[12]: [N_dir, W_dir],
        c[1]:  [N_dir, W_dir, S_dir],
        c[2]:  [N_dir, S_dir],
        c[15]: [S_dir, W_dir],
    }
    for card_obj, dirs in card_dirs.items():
        for d in dirs:
            p.set_initial_value(connects(card_obj, d), True)

    # ── Movement ──────────────────────────────────────────────────────────────

    def _move(name, r_type, c_type, dr, dc, from_dir, to_dir):
        act = InstantaneousAction(name, r=r_type, c=c_type)
        r, col = act.parameter('r'), act.parameter('c')
        act.add_precondition(Equals(robot_at, card_at[r][col]))
        act.add_precondition(connects(card_at[r][col],          from_dir))
        act.add_precondition(connects(card_at[r + dr][col + dc], to_dir))
        act.add_effect(robot_at, card_at[r + dr][col + dc])
        p.add_action(act)

    _move('move_north', IntType(1, 3), idx_t, -1,  0, N_dir, S_dir)
    _move('move_south', IntType(0, 2), idx_t, +1,  0, S_dir, N_dir)
    _move('move_east',  idx_t, IntType(0, 2),  0, +1, E_dir, W_dir)
    _move('move_west',  idx_t, IntType(1, 3),  0, -1, W_dir, E_dir)

    # ── Rotation ──────────────────────────────────────────────────────────────

    def _rotate_col(name, row_order):
        act = InstantaneousAction(name, col=idx_t)
        col = act.parameter('col')
        for row in range(4):
            act.add_precondition(Not(Equals(robot_at, card_at[row][col])))
        for dst, src in enumerate(row_order):
            act.add_effect(card_at[dst][col], card_at[src][col])
        p.add_action(act)

    def _rotate_row(name, col_order):
        act = InstantaneousAction(name, row=idx_t)
        row = act.parameter('row')
        for col in range(4):
            act.add_precondition(Not(Equals(robot_at, card_at[row][col])))
        for dst, src in enumerate(col_order):
            act.add_effect(card_at[row][dst], card_at[row][src])
        p.add_action(act)

    _rotate_col('rotate_col_up',    [1, 2, 3, 0])  # dst 0←src 1, 1←2, 2←3, 3←0
    _rotate_col('rotate_col_down',  [3, 0, 1, 2])
    _rotate_row('rotate_row_left',  [1, 2, 3, 0])
    _rotate_row('rotate_row_right', [3, 0, 1, 2])

    # ── Goal ──────────────────────────────────────────────────────────────────
    # robot_at == card_at[3][3] (= card_15) and card_15 has S connection
    p.add_goal(Equals(robot_at, card_at[3][3]))
    p.add_goal(connects(card_at[3][3], S_dir))
    return p
