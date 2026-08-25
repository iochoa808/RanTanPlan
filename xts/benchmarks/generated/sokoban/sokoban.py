"""
Shared generator for the 20 real Sokoban benchmark instances (IPC 2011
benchmark, grids up to ~10x10 -- matches the paper's Table 1 "Sokoban (20)"
row; the paper's body text says "39 instances" but the actual repo only
ships instances.txt/probs/ for 20 (i_1..i_20), so 20 is used here as the
verified ground truth).

Level parsing is ported from
~/unified-planning/docs/extensions/domains/sokoban/read_instance.py
(originally a standalone script invoked via `subprocess` from Sokoban.py
against a broken absolute path "/probs/{file}.txt" that doesn't exist in
this checkout -- ported directly to a function here instead of trying to
fix the subprocess plumbing). The parsing heuristic: strip the outermost
matrix row/column (assumed pure wall margin), then for each remaining row
find where '#' characters start/end to determine the actual in-bounds
playable rectangle per row and per column (Sokoban levels are often
irregular/non-rectangular ASCII art) -- cells outside those bounds become
`undefined_positions` in RTP's array fluent (mirroring RushHour's use of
the same array feature for irregular grids).

Symbols: '#'=wall (not a cell), '@'=player start, '$'=box start, '.'=goal,
'*'=box already on a goal, ' '=empty in-bounds floor.

Replicates the UP model in
~/unified-planning/docs/extensions/domains/sokoban/Sokoban.py: a
grid(Pattern)[r][c] array-of-predicate fluent (Pattern in {{P, B}} for
Player/Box), 4 move actions + 4 push actions (move cost 0, push cost 1,
matching the paper's cost metric), goal: a Box on every goal cell.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))

from unified_planning.shortcuts import *


def _parse_level(lines):
    matrix = [list(line.rstrip("\n")) for line in lines]

    row_limits = {}
    for i, row in enumerate(matrix[1:-1]):
        if all(x == '#' for x in row) or '#' not in row:
            row_limits[i] = (0, len(row) - 3)
        else:
            start = row.index('#')
            while row[start] == '#':
                start += 1
            end = len(row) - 1
            while row[end] == '#':
                end -= 1
            row_limits[i] = (start - 1, end - 1)

    num_cols = max(len(row) for row in matrix)
    padded_matrix = [list(''.join(row).ljust(num_cols)) for row in matrix]
    columns = [[row[col_idx] for row in padded_matrix] for col_idx in range(num_cols)]

    col_limits = {}
    for i, col in enumerate(columns[1:-1]):
        if all(x == '#' for x in col) or '#' not in col:
            col_limits[i] = (0, len(col) - 3)
        else:
            start = col.index('#')
            while col[start] == '#':
                start += 1
            end = len(col) - 1 - col[::-1].index('#')
            while col[end] == '#':
                end -= 1
            col_limits[i] = (start - 1, end - 1)

    initial_state = {}
    defined_positions = []
    goal_positions = []

    for ri, row in enumerate(matrix[1:-1]):
        for ci, cell in enumerate(row[1:-1]):
            if (cell != '#'
                    and col_limits[ci][0] <= ri <= col_limits[ci][1]
                    and row_limits[ri][0] <= ci <= row_limits[ri][1]):
                defined_positions.append((ri, ci))
                if cell == '@':
                    initial_state[(ri, ci)] = 'P'
                elif cell == '$':
                    initial_state[(ri, ci)] = 'B'
                elif cell == '.':
                    goal_positions.append((ri, ci))
                elif cell == '*':
                    goal_positions.append((ri, ci))
                    initial_state[(ri, ci)] = 'B'

    rows = max(x[0] for x in defined_positions) + 1
    columns_n = max(x[1] for x in defined_positions) + 1
    all_positions = {(r, c) for r in range(rows) for c in range(columns_n)}
    undefined_positions = sorted(all_positions - set(defined_positions))

    return initial_state, undefined_positions, goal_positions, rows, columns_n


def build_sokoban(name: str, level_text: str):
    lines = level_text.splitlines()
    initial_state, undefined_positions, goal_positions, rows, columns = _parse_level(lines)

    p = Problem(name)

    Pattern = UserType('Pattern')
    P = Object('P', Pattern)
    B = Object('B', Pattern)
    pattern_by_symbol = {'P': P, 'B': B}
    p.add_objects([P, B])

    grid = Fluent('grid', ArrayType(rows, ArrayType(columns)), p=Pattern, undefined_positions=undefined_positions)
    p.add_fluent(grid, default_initial_value=False)
    # Whole-array assignment (per Pattern value), not per-cell
    # grid(pattern)[r][c] indexing -- the per-cell form hits the same
    # "AssertionError: fluent field must be a fluent" bug as
    # labyrinth_ipc.py's card_at and rush_hour.py's occupied.
    for symbol, obj in pattern_by_symbol.items():
        p_grid = [[(r, c) in initial_state and initial_state[(r, c)] == symbol
                   for c in range(columns)] for r in range(rows)]
        p.set_initial_value(grid(obj), p_grid)

    move_right = InstantaneousAction('move_right', r=IntType(0, rows - 1), c=IntType(0, columns - 1))
    r, c = move_right.parameter('r'), move_right.parameter('c')
    move_right.add_precondition(grid(P)[r][c])
    move_right.add_precondition(Not(grid(P)[r][c + 1]))
    move_right.add_precondition(Not(grid(B)[r][c + 1]))
    move_right.add_effect(grid(P)[r][c + 1], True)
    move_right.add_effect(grid(P)[r][c], False)

    move_left = InstantaneousAction('move_left', r=IntType(0, rows - 1), c=IntType(0, columns - 1))
    r, c = move_left.parameter('r'), move_left.parameter('c')
    move_left.add_precondition(grid(P)[r][c])
    move_left.add_precondition(Not(grid(P)[r][c - 1]))
    move_left.add_precondition(Not(grid(B)[r][c - 1]))
    move_left.add_effect(grid(P)[r][c - 1], True)
    move_left.add_effect(grid(P)[r][c], False)

    move_up = InstantaneousAction('move_up', r=IntType(0, rows - 1), c=IntType(0, columns - 1))
    r, c = move_up.parameter('r'), move_up.parameter('c')
    move_up.add_precondition(grid(P)[r][c])
    move_up.add_precondition(Not(grid(P)[r - 1][c]))
    move_up.add_precondition(Not(grid(B)[r - 1][c]))
    move_up.add_effect(grid(P)[r - 1][c], True)
    move_up.add_effect(grid(P)[r][c], False)

    move_down = InstantaneousAction('move_down', r=IntType(0, rows - 1), c=IntType(0, columns - 1))
    r, c = move_down.parameter('r'), move_down.parameter('c')
    move_down.add_precondition(grid(P)[r][c])
    move_down.add_precondition(Not(grid(P)[r + 1][c]))
    move_down.add_precondition(Not(grid(B)[r + 1][c]))
    move_down.add_effect(grid(P)[r + 1][c], True)
    move_down.add_effect(grid(P)[r][c], False)

    push_box_right = InstantaneousAction('push_box_right', r=IntType(0, rows - 1), c=IntType(0, columns - 1))
    r, c = push_box_right.parameter('r'), push_box_right.parameter('c')
    push_box_right.add_precondition(grid(P)[r][c])
    push_box_right.add_precondition(grid(B)[r][c + 1])
    push_box_right.add_precondition(Not(grid(P)[r][c + 2]))
    push_box_right.add_precondition(Not(grid(B)[r][c + 2]))
    push_box_right.add_effect(grid(P)[r][c + 1], True)
    push_box_right.add_effect(grid(B)[r][c + 2], True)
    push_box_right.add_effect(grid(P)[r][c], False)
    push_box_right.add_effect(grid(B)[r][c + 1], False)

    push_box_left = InstantaneousAction('push_box_left', r=IntType(0, rows - 1), c=IntType(0, columns - 1))
    r, c = push_box_left.parameter('r'), push_box_left.parameter('c')
    push_box_left.add_precondition(grid(P)[r][c])
    push_box_left.add_precondition(grid(B)[r][c - 1])
    push_box_left.add_precondition(Not(grid(P)[r][c - 2]))
    push_box_left.add_precondition(Not(grid(B)[r][c - 2]))
    push_box_left.add_effect(grid(P)[r][c - 1], True)
    push_box_left.add_effect(grid(B)[r][c - 2], True)
    push_box_left.add_effect(grid(P)[r][c], False)
    push_box_left.add_effect(grid(B)[r][c - 1], False)

    push_box_up = InstantaneousAction('push_box_up', r=IntType(0, rows - 1), c=IntType(0, columns - 1))
    r, c = push_box_up.parameter('r'), push_box_up.parameter('c')
    push_box_up.add_precondition(grid(P)[r][c])
    push_box_up.add_precondition(grid(B)[r - 1][c])
    push_box_up.add_precondition(Not(grid(P)[r - 2][c]))
    push_box_up.add_precondition(Not(grid(B)[r - 2][c]))
    push_box_up.add_effect(grid(P)[r - 1][c], True)
    push_box_up.add_effect(grid(B)[r - 2][c], True)
    push_box_up.add_effect(grid(P)[r][c], False)
    push_box_up.add_effect(grid(B)[r - 1][c], False)

    push_box_down = InstantaneousAction('push_box_down', r=IntType(0, rows - 1), c=IntType(0, columns - 1))
    r, c = push_box_down.parameter('r'), push_box_down.parameter('c')
    push_box_down.add_precondition(grid(P)[r][c])
    push_box_down.add_precondition(grid(B)[r + 1][c])
    push_box_down.add_precondition(Not(grid(P)[r + 2][c]))
    push_box_down.add_precondition(Not(grid(B)[r + 2][c]))
    push_box_down.add_effect(grid(P)[r + 1][c], True)
    push_box_down.add_effect(grid(B)[r + 2][c], True)
    push_box_down.add_effect(grid(P)[r][c], False)
    push_box_down.add_effect(grid(B)[r + 1][c], False)

    p.add_actions([move_right, move_left, move_up, move_down,
                   push_box_right, push_box_left, push_box_up, push_box_down])

    for r, c in goal_positions:
        p.add_goal(grid(B)[r][c])

    costs = {
        move_right: Int(0), move_left: Int(0), move_up: Int(0), move_down: Int(0),
        push_box_right: Int(1), push_box_left: Int(1), push_box_up: Int(1), push_box_down: Int(1),
    }
    p.add_quality_metric(MinimizeActionCosts(costs))
    return p


LEVELS = {
    'i_1': '  ########\n  #  # . #\n  #   .*.#\n  #  # * #\n####$##.##\n#      $ #\n# $ ## $ #\n#   @#   #\n##########',
    'i_2': '       #####\n########   #\n#.   .  @#.#\n#  ###     #\n## $  #    #\n # $   #####\n # $#  #\n ## #  #\n  #   ##\n  #####',
    'i_3': ' ###########\n##.......  #\n# $$$$$$$@ #\n#   # # # ##\n# # #     #\n#   #######\n#####',
    'i_4': ' #######\n##     ##\n#  $ $  #\n# $ $ $ #\n## ### ####\n #@  .....#\n ##     ###\n  #######\n',
    'i_5': ' ########\n #      #\n #@   $ #\n## ###$ #\n# .....###\n# $ $ $  #\n###### # #\n     #   #\n     #####',
    'i_6': ' #####\n##   ##\n#  $  ##\n# $ $  ##\n###$# . ##\n  # # .  #\n ## ##.  #\n # @  . ##\n #   #  #\n ########',
    'i_7': ' ####\n##  #\n#. $#\n#.$ #\n#.$ #\n#.$ #\n#. $##\n#   @#\n##   #\n #####',
    'i_8': '######\n#    ###\n#  # $ #\n#  $ @ #\n## ## #####\n#  #......#\n# $ $ $ $ #\n##   ######\n #####',
    'i_9': ' ############################\n #                          #\n # ######################## #\n # #                      # #\n # # #################### # #\n # # #                  # # #\n # # # ################ # # #\n # # # #              # # # #\n # # # # ############ # # # #\n # # # # #            # # # #\n # # # # # ############ # # #\n # # # # #              # # #\n # # # # ################ # #\n # # # #                  # #\n##$# # #################### #\n#. @ #                      #\n#############################',
    'i_10': '       ####\n      ##  ###\n####  #  $  #\n#  #### $ $ #\n#   ..# #$  #\n#  #   @  ###\n## #..# ###\n # ## # #\n #      #\n ########',
    'i_11': '########\n#      #\n# $ $$ ########\n##### @##. .  #\n    #$  # .   #\n    #   #. . ##\n    #$# ## # #\n    #        #\n    #  ###  ##\n    #  # ####\n    ####',
    'i_12': '     ####\n #####  #\n #     $#######\n## ## ..#  ...#\n# $ $$#$  @   #\n#        ###  #\n#######  # ####\n      ####',
    'i_13': ' #### ####\n##  ###  ##\n#   # #   #\n#  *. .*  #\n###$   $###\n #   @   #\n###$   $###\n#  *. .*  #\n#   # #   #\n##  ###  ##\n #### ####',
    'i_14': ' ######\n # .  #\n##$.# #\n#  *  #\n# ..###\n##$ # #####\n## ## #   #\n#  #### # #\n#   @ $ $ #\n##  #     #\n ##########',
    'i_15': '######## #####\n#  #   ###   #\n#      ## $  #\n#.# @ ## $  ##\n#.#   # $  ##\n#.#    $  ##\n#. ## #####\n##    #\n ######',
    'i_16': '   #####\n   # @ #\n  ##   ##\n###.$$$.###\n#  $...$  #\n#  $.#.$  #\n#  $...$  #\n###.$$$.###\n  ##   ##\n   #   #\n   #####',
    'i_17': ' #### ####\n #  ###  ##\n #      @ #\n##..###   #\n#      #  #\n#...#$  # #\n# ## $$ $ #\n#  $    ###\n####  ###\n   ####',
    'i_18': '  #######\n# #     #\n# # # # #\n  # @ $ #\n### ### #\n#   ### #\n# $  ##.#\n## $  #.#\n ## $  .#\n# ## $#.#\n## ## #.#\n### #   #\n### #####',
    'i_19': ' #######\n #  . .###\n # . . . #\n### #### #\n#  @$  $ #\n#  $$  $ #\n####   ###\n   #####',
    'i_20': ' #######\n##  .  ##\n# .$$$. #\n# $. .$ #\n#.$ @ $.#\n# $. .$ #\n# .$$$. #\n##  .  ##\n #######'
}
