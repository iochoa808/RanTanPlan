"""
Labyrinth — v2, 4×4 grid storing direction-sets, integer robot position.

card_at: ArrayType(4, ArrayType(4, SetType(Direction)))
robot_row, robot_col: idx (integer [0,3]) fluents
No card objects; no connections predicate — membership in the cell's set
replaces the separate connections(card, dir) predicate.
Rotation precondition: single inequality on robot_row or robot_col.
PDDL-XTS counterpart: PDDL-XTS/labyrinth_v2/instances/i0.pddl
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('labyrinth_i0_v2')

    Direction = UserType('Direction')
    N_dir = Object('N', Direction); S_dir = Object('S', Direction)
    E_dir = Object('E', Direction); W_dir = Object('W', Direction)
    p.add_objects([N_dir, S_dir, E_dir, W_dir])

    idx_t = IntType(0, 3)
    card_at   = Fluent('card_at',   ArrayType(4, ArrayType(4, SetType(Direction))))
    robot_row = Fluent('robot_row', idx_t)
    robot_col = Fluent('robot_col', idx_t)
    p.add_fluent(card_at)
    p.add_fluent(robot_row, default_initial_value=Int(0))
    p.add_fluent(robot_col, default_initial_value=Int(0))
    p.set_initial_value(robot_row, Int(0))
    p.set_initial_value(robot_col, Int(0))

    # Grid: same card layout as v1; each cell stores its open-directions set.
    # row 0: {W,E}    {S,W}    {N,W,S,E}  {N,W,E}
    # row 1: {N,W,E}  {S,W}    {N,S}      {S,E}
    # row 2: {N,S,E}  {S,W}    {N,S,E}    {S,E}
    # row 3: {N,W}    {N,W,S}  {N,S}      {S,W}
    N, S, E, W = N_dir, S_dir, E_dir, W_dir
    p.set_initial_value(card_at, [
        [{W,E},     {S,W},     {N,W,S,E},  {N,W,E}  ],
        [{N,W,E},   {S,W},     {N,S},      {S,E}    ],
        [{N,S,E},   {S,W},     {N,S,E},    {S,E}    ],
        [{N,W},     {N,W,S},   {N,S},      {S,W}    ],
    ])

    # ── Movement ──────────────────────────────────────────────────────────────

    def _move(name, r_type, c_type, dr, dc, from_dir, to_dir):
        act = InstantaneousAction(name, r=r_type, c=c_type)
        r, col = act.parameter('r'), act.parameter('c')
        act.add_precondition(Equals(robot_row, r))
        act.add_precondition(Equals(robot_col, col))
        act.add_precondition(SetMember(from_dir, card_at[r][col]))
        act.add_precondition(SetMember(to_dir,   card_at[r + dr][col + dc]))
        if dr == -1: act.add_effect(robot_row, Minus(robot_row, 1))
        if dr ==  1: act.add_effect(robot_row, Plus(robot_row,  1))
        if dc == -1: act.add_effect(robot_col, Minus(robot_col, 1))
        if dc ==  1: act.add_effect(robot_col, Plus(robot_col,  1))
        p.add_action(act)

    _move('move_north', IntType(1, 3), idx_t,   -1,  0, N_dir, S_dir)
    _move('move_south', IntType(0, 2), idx_t,   +1,  0, S_dir, N_dir)
    _move('move_east',  idx_t, IntType(0, 2),    0, +1, E_dir, W_dir)
    _move('move_west',  idx_t, IntType(1, 3),    0, -1, W_dir, E_dir)

    # ── Rotation ──────────────────────────────────────────────────────────────
    # Direction sets live in the cells, so rotating a row/col carries
    # each cell's open-dirs with it.  Single inequality check suffices.

    def _rotate_col(name, row_order):
        act = InstantaneousAction(name, col=idx_t)
        col = act.parameter('col')
        act.add_precondition(Not(Equals(robot_col, col)))
        for dst, src in enumerate(row_order):
            act.add_effect(card_at[dst][col], card_at[src][col])
        p.add_action(act)

    def _rotate_row(name, col_order):
        act = InstantaneousAction(name, row=idx_t)
        row = act.parameter('row')
        act.add_precondition(Not(Equals(robot_row, row)))
        for dst, src in enumerate(col_order):
            act.add_effect(card_at[row][dst], card_at[row][src])
        p.add_action(act)

    _rotate_col('rotate_col_up',    [1, 2, 3, 0])
    _rotate_col('rotate_col_down',  [3, 0, 1, 2])
    _rotate_row('rotate_row_left',  [1, 2, 3, 0])
    _rotate_row('rotate_row_right', [3, 0, 1, 2])

    # ── Goal ──────────────────────────────────────────────────────────────────
    p.add_goal(Equals(robot_row, 3))
    p.add_goal(Equals(robot_col, 3))
    p.add_goal(SetMember(S_dir, card_at[3][3]))
    return p
