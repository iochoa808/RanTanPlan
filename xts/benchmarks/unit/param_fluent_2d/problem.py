"""
Per-player 2D grid — parameterized 2D array fluent: grid(?p) for each player.
write_cell(p, r, c, v): write v into player p's cell [r][c].
Initial: all grids zero; goal: grid(player1)[0][1]=5.
Plan: write_cell(player1, 0, 1, 5)  →  1 step.
PDDL-XTS: param_fluent_2d
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('player_grid')

    Player  = UserType('Player')
    player1 = Object('player1', Player)
    player2 = Object('player2', Player)
    p.add_objects([player1, player2])

    val_t  = IntType(0, 9)
    row_t  = IntType(0, 1)
    col_t  = IntType(0, 1)

    grid = Fluent('grid', ArrayType(2, ArrayType(2, val_t)), p=Player)
    p.add_fluent(grid)

    p.set_initial_value(grid(player1), [[0, 0], [0, 0]])
    p.set_initial_value(grid(player2), [[0, 0], [0, 0]])

    write_cell = InstantaneousAction('write_cell', pl=Player, r=row_t, c=col_t, v=val_t)
    pl, r, c, v = (write_cell.parameter('pl'), write_cell.parameter('r'),
                   write_cell.parameter('c'), write_cell.parameter('v'))
    write_cell.add_precondition(Equals(grid(pl)[r][c], 0))
    write_cell.add_effect(grid(pl)[r][c], v)
    p.add_action(write_cell)

    p.add_goal(Equals(grid(player1)[0][1], 5))
    return p
