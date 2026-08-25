"""
Forall over an array column, next to an action sharing the parameter name.
PDDL-XTS: forall_column_guard

`rotate_col_up` guards column c with Forall(k, marker != board[k][c]) while the
earlier `scan` action declares a same-named, same-typed parameter c. IPAR grounds
scan first and both actions share one interned parameter node, so a substitution
cache that ignores the name->slot mapping makes c resolve to the range variable:
every rotate_col_up_k then guards the diagonal board[j][j] instead of column k.

marker=4 sits on board[1][1], so the diagonal reading blocks every rotation and
the goal turns unreachable; the correct reading leaves rotate_col_up(0) applicable.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('forall_column_guard')

    idx_t = IntType(0, 2)
    val_t = IntType(0, 8)
    count_t = IntType(0, 3)

    board = Fluent('board', ArrayType(3, ArrayType(3, val_t)))
    marker = Fluent('marker', val_t)
    probes = Fluent('probes', count_t)
    p.add_fluent(board)
    p.add_fluent(marker)
    p.add_fluent(probes)
    p.set_initial_value(board, [[0, 1, 2], [3, 4, 5], [6, 7, 8]])
    p.set_initial_value(marker, 4)
    p.set_initial_value(probes, 0)

    # Added first so its groundings reach the substitution cache first.
    scan = InstantaneousAction('scan', r=idx_t, c=idx_t)
    r, c = scan.parameter('r'), scan.parameter('c')
    scan.add_precondition(Equals(board[r][c], marker))
    scan.add_increase_effect(probes, 1)
    p.add_action(scan)

    rotate = InstantaneousAction('rotate_col_up', c=idx_t)
    c = rotate.parameter('c')
    k = RangeVariable('k', 0, 2)
    rotate.add_precondition(Forall(Not(Equals(marker, board[k][c])), k))
    rotate.add_effect(board[0][c], board[1][c])
    rotate.add_effect(board[1][c], board[2][c])
    rotate.add_effect(board[2][c], board[0][c])
    p.add_action(rotate)

    p.add_goal(Equals(board[0][0], 3))
    return p
