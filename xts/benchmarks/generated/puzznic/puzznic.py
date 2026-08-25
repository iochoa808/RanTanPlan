"""
Shared generator for the 39 real Puzznic benchmark instances (grids up to
~10x7, matching the paper's "Puzznic (39)" row -- both the handcrafted
model and this one use derived predicates, integrated through UP framework
support for Axiom/DerivedBoolType).

Level parsing is ported from
~/unified-planning/docs/extensions/domains/puzznic/read_instance.py
(originally invoked via `subprocess` from Puzznic.py against a broken path
"/probs/{{file}}.prob" that doesn't exist in this checkout -- ported to a
function here instead). Symbols: '#'=wall, ' '=free cell (becomes 'F' /
Free pattern), any other single char = a colored block pattern.

Replicates the UP model in
~/unified-planning/docs/extensions/domains/puzznic/Puzznic.py: a
patterned(Pattern)[r][c] array-of-predicate fluent (10 possible Pattern
colors incl. Free), two derived predicates (falling_flag: some block has a
Free cell directly below it and should fall; matching_flag: some two
same-colored adjacent blocks exist and should be cleared), move/fall
actions gated on NOT falling_flag/matching_flag (blocks must settle and
clear before more moves), and a single 0-ary 'matching_blocks' action
whose conditional forall effect clears every matched block at once. Goal:
every in-bounds cell is Free (fully cleared board).

Two bugs found in the reference script (confirmed by running it
unmodified) fixed here, both instances of the same underlying UP-version
incompatibility already seen in labyrinth_ipc.py/rush_hour.py/sokoban.py:
per-cell `patterned[r][c](pattern)` array-then-parameter indexing order
must instead be `patterned(pattern)[r][c]` (parameter-then-array), and
per-cell set_initial_value must be a whole-array assignment instead.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))

from unified_planning.shortcuts import *


def _parse_level(lines):
    matrix = [list(line.rstrip("\n")) for line in lines]

    initial_state = {}
    undefined = []
    row_index = 0
    for row in matrix[1:-1]:
        if row[0] == '#' and row[1] != '#':
            first_wall_index = -1
        else:
            first_wall_index = row.index('#') - 1
        if row[-1] == '#' and row[-2] != '#':
            last_wall_index = len(row[1:-1])
        else:
            last_wall_index = len(row) - 1 - row[::-1].index('#')
        for col_index, cell in enumerate(row[1:-1]):
            if col_index <= first_wall_index or col_index >= last_wall_index or cell == '#':
                undefined.append((row_index, col_index))
            elif cell != ' ':
                initial_state[(row_index, col_index)] = cell
        row_index += 1

    num_rows = row_index
    num_cols = len(matrix[0]) - 2
    return initial_state, undefined, num_rows, num_cols


def build_puzznic(name: str, level_text: str):
    inlines = level_text.splitlines(keepends=True)
    lines = []
    for l in inlines:
        if l in ('\n', ''):
            break
        lines.append(l)
    initial_state, undefined, rows, columns = _parse_level(lines)

    p = Problem(name)

    Pattern = UserType('Pattern')
    F = Object('F', Pattern)
    B = Object('B', Pattern)
    Y = Object('Y', Pattern)
    G = Object('G', Pattern)
    R = Object('R', Pattern)
    L = Object('L', Pattern)
    O = Object('O', Pattern)
    V = Object('V', Pattern)
    Pk = Object('P', Pattern)
    C = Object('C', Pattern)
    pattern_by_symbol = {'F': F, 'B': B, 'Y': Y, 'G': G, 'R': R, 'L': L, 'O': O, 'V': V, 'P': Pk, 'C': C}
    p.add_object(F)

    patterned = Fluent('patterned', ArrayType(rows, ArrayType(columns)), p=Pattern, undefined_positions=undefined)
    p.add_fluent(patterned, default_initial_value=False)

    used_symbols = set(initial_state.values()) | {'F'}
    for symbol in used_symbols:
        obj = pattern_by_symbol[symbol]
        if not p.has_object(symbol):
            p.add_object(obj)
        is_free_cell = (symbol == 'F')
        grid = [[False] * columns for _ in range(rows)]
        for r in range(rows):
            for c in range(columns):
                if (r, c) in undefined:
                    continue
                cell_symbol = initial_state.get((r, c))
                if cell_symbol == symbol or (cell_symbol is None and is_free_cell):
                    grid[r][c] = True
        p.set_initial_value(patterned(obj), grid)

    falling_flag = Fluent('falling_flag', DerivedBoolType())
    p.add_fluent(falling_flag, default_initial_value=False)
    matching_flag = Fluent('matching_flag', DerivedBoolType())
    p.add_fluent(matching_flag, default_initial_value=False)
    # Unlike robot_at/hand elsewhere in this batch, UP itself refuses an
    # explicit set_initial_value on a *derived* fluent ("You cannot set
    # the initial value of a derived fluent!") -- there is no workaround
    # at this level. RTP's native pipeline then fails during compilation
    # ("Object fluent(s) missing initial value... falling_flag,
    # matching_flag"), i.e. RTP's native ingestion does not support
    # Axiom/DerivedBoolType at all. This looks like a hard native-pipeline
    # limitation, not a fixable modeling bug -- the paper itself only
    # pairs derived predicates with FD (via the 'up' pipeline), never
    # with a native/SAT-based solver, which matches what we're seeing.

    axiom_falling = Axiom('axiom_falling')
    axiom_falling.set_head(falling_flag)
    i = RangeVariable('i', 1, rows - 1)
    j = RangeVariable('j', 0, columns - 1)
    axiom_falling.add_body_condition(
        Exists(And(Not(patterned(F)[i - 1][j]), patterned(F)[i][j]), i, j)
    )
    p.add_axiom(axiom_falling)

    axiom_matching = Axiom('axiom_matching')
    axiom_matching.set_head(matching_flag)
    i = RangeVariable('i', 0, rows - 1)
    j = RangeVariable('j', 0, columns - 2)
    pv = Variable('p', Pattern)
    matching_horizontal = Exists(
        And(patterned(pv)[i][j], patterned(pv)[i][j + 1], Not(Equals(pv, F))), i, j, pv
    )
    i = RangeVariable('i', 0, rows - 2)
    j = RangeVariable('j', 0, columns - 1)
    pv = Variable('p', Pattern)
    matching_vertical = Exists(
        And(patterned(pv)[i][j], patterned(pv)[i + 1][j], Not(patterned(F)[i][j])), i, j, pv
    )
    axiom_matching.add_body_condition(Or(matching_horizontal, matching_vertical))
    p.add_axiom(axiom_matching)

    move_block_right = InstantaneousAction('move_block_right', p_=Pattern, r=IntType(0, rows - 1), c=IntType(0, columns - 1))
    p_, r, c = move_block_right.parameter('p_'), move_block_right.parameter('r'), move_block_right.parameter('c')
    move_block_right.add_precondition(Not(falling_flag))
    move_block_right.add_precondition(Not(matching_flag))
    move_block_right.add_precondition(patterned(p_)[r][c])
    move_block_right.add_precondition(Not(Equals(p_, F)))
    move_block_right.add_precondition(patterned(F)[r][c + 1])
    move_block_right.add_effect(patterned(F)[r][c], True)
    move_block_right.add_effect(patterned(p_)[r][c + 1], True)
    move_block_right.add_effect(patterned(p_)[r][c], False)
    move_block_right.add_effect(patterned(F)[r][c + 1], Or(patterned(p_)[r][c], patterned(p_)[r][c + 1]))
    p.add_action(move_block_right)

    move_block_left = InstantaneousAction('move_block_left', p_=Pattern, r=IntType(0, rows - 1), c=IntType(0, columns - 1))
    p_, r, c = move_block_left.parameter('p_'), move_block_left.parameter('r'), move_block_left.parameter('c')
    move_block_left.add_precondition(Not(falling_flag))
    move_block_left.add_precondition(Not(matching_flag))
    move_block_left.add_precondition(patterned(p_)[r][c])
    move_block_left.add_precondition(Not(Equals(p_, F)))
    move_block_left.add_precondition(patterned(F)[r][c - 1])
    move_block_left.add_effect(patterned(F)[r][c], True)
    move_block_left.add_effect(patterned(p_)[r][c - 1], True)
    move_block_left.add_effect(patterned(p_)[r][c], False)
    move_block_left.add_effect(patterned(F)[r][c - 1], False)
    p.add_action(move_block_left)

    fall_block = InstantaneousAction('fall_block', p_=Pattern, r=IntType(0, rows - 2), c=IntType(0, columns - 1))
    p_, r, c = fall_block.parameter('p_'), fall_block.parameter('r'), fall_block.parameter('c')
    fall_block.add_precondition(falling_flag)
    fall_block.add_precondition(patterned(p_)[r][c])
    fall_block.add_precondition(Not(Equals(p_, F)))
    fall_block.add_precondition(patterned(F)[r + 1][c])
    fall_block.add_effect(patterned(F)[r][c], True)
    fall_block.add_effect(patterned(p_)[r + 1][c], True)
    fall_block.add_effect(patterned(p_)[r][c], False)
    fall_block.add_effect(patterned(F)[r + 1][c], False)
    p.add_action(fall_block)

    matching_blocks = InstantaneousAction('matching_blocks')
    matching_blocks.add_precondition(Not(falling_flag))
    matching_blocks.add_precondition(matching_flag)
    # The original single forall (i,j over the *full* board with
    # unbounded i+1/i-1/j+1/j-1 reads) hits a UPTypeError in this UP
    # version: RangeVariable-derived array-index bounds are checked
    # strictly (both directions), unlike plain action-parameter
    # arithmetic (see e.g. move_block_right's c+1, which is fine because
    # c is a parameter, not a RangeVariable). Split into 4 directional
    # effects, each with its own RangeVariable range capped so its single
    # +1/-1 read stays in-bounds -- every cell still independently checks
    # all 4 neighbors (each direction contributes its own add_effect call
    # covering the (rows x columns) sub-rectangle where that neighbor
    # exists), so the semantics are unchanged: a cell clears iff it has
    # at least one same-colour orthogonal neighbour.
    pv = Variable('p', Pattern)

    i = RangeVariable('i', 0, rows - 1)
    j = RangeVariable('j', 0, columns - 2)
    matching_blocks.add_effect(patterned(F)[i][j], True, condition=And(
        Not(Equals(pv, F)), patterned(pv)[i][j], patterned(pv)[i][j + 1]
    ), forall=[i, j, pv])

    i = RangeVariable('i', 0, rows - 1)
    j = RangeVariable('j', 1, columns - 1)
    matching_blocks.add_effect(patterned(F)[i][j], True, condition=And(
        Not(Equals(pv, F)), patterned(pv)[i][j], patterned(pv)[i][j - 1]
    ), forall=[i, j, pv])

    i = RangeVariable('i', 0, rows - 2)
    j = RangeVariable('j', 0, columns - 1)
    matching_blocks.add_effect(patterned(F)[i][j], True, condition=And(
        Not(Equals(pv, F)), patterned(pv)[i][j], patterned(pv)[i + 1][j]
    ), forall=[i, j, pv])

    i = RangeVariable('i', 1, rows - 1)
    j = RangeVariable('j', 0, columns - 1)
    matching_blocks.add_effect(patterned(F)[i][j], True, condition=And(
        Not(Equals(pv, F)), patterned(pv)[i][j], patterned(pv)[i - 1][j]
    ), forall=[i, j, pv])

    p.add_action(matching_blocks)

    for i in range(rows):
        for j in range(columns):
            if (i, j) not in undefined:
                p.add_goal(patterned(F)[i][j])

    costs = {
        move_block_right: Int(1), move_block_left: Int(1),
        matching_blocks: Int(0), fall_block: Int(0),
    }
    p.add_quality_metric(MinimizeActionCosts(costs))
    return p


LEVELS = {
    'puzznic1': '####\n# R#\n#R##\n####',
    'puzznic2': '#####\n#R R#\n#P B#\n## ##\n#   #\n#B P#\n#####',
    'puzznic4': '#####\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#   #\n#R R#\n#P B#\n## ##\n#   #\n#B P#\n#####',
    'puzznic5': '######\n# R  #\n#RBR #\n#BGB #\n#GBR #\n######',
    'puzznic6': ' #### \n##R ##\n#PB P#\n#BPRB#\n###B##\n #### \n',
    'puzznic7': '######\n#    #\n# Y  #\n# G  #\n#GB  #\n##Y B#\n######',
    'puzznic8': '######\n# R  #\n#RG  #\n#GR  #\n#RB  #\n### B#\n######',
    'puzznic9': '######\n# G  #\n#GR  #\n#RG  #\n#GR  #\n#RG  #\n#GB  #\n### B#\n######',
    'puzznic10': '######\n## O #\n## # #\n#  O #\n## ###\n## ###\n## ###\n## O##\n## ###\n## ###\n######',
    'puzznic11': '#######\n#     #\n# G   #\n#YR   #\n#GB   #\n#BR Y #\n#######',
    'puzznic12': '#######\n#     #\n# G   #\n#YR   #\n#GB   #\n#BR O #\n#YO####\n#######',
    'puzznic13': '   #####\n ### L #\n## B GB#\n#  ##R##\n#R# #PG#\n#P# ##L#\n########',
    'puzznic14': '########\n#BG    #\n### P  #\n#   #  #\n#   # B#\n# R  ###\n# GP  R#\n########',
    'puzznic15': '   ##   \n  #B #  \n #BG  # \n#LGR   #\n#PRL   #\n #BP  # \n  #L #  \n   ##   ',
    'puzznic16': '   ##   \n  #CB#  \n #BGLP# \n# GCRL #\n# B##P #\n #RGPR##\n  #PG#  \n   ##   ',
    'puzznic17': '####### \n#R#B#R# \n#P#P#P# \n#R B B# \n#### ###\n #  P  #\n #  # P#\n  #   # \n   # #  \n   # #  \n    #   ',
    'puzznic18': '    ####\n    #  #\n    #  #\n    #O #\n    #L #\n    #B #\n  ###PG#\n  #  RP#\n### #CG#\n#O  BRC#\n#YGLGBY#\n########',
    'puzznic19': '########\n##    ##\n#  G   #\n#G L O #\n#L Y C #\n#G O O #\n#C C Y #\n#YOGYLG#\n#CGCLYO#\n#GLGYOY#\n#CYCLCO#\n########',
    'puzznic20': '#########\n#       #\n#BPB  PB#\n##### ###\n#########',
    'puzznic21': '#########\n#ROYGB  #\n######  #\n#       #\n#  ######\n#  ROYGB#\n#########',
    'puzznic22': '#########\n#BP   RG#\n#### ####\n#       #\n#       #\n#B#P#R#G#\n#########',
    'puzznic23': '#########\n#PG     #\n #B     #\n  #PB   #\n   #RB  #\n   ###  #\n     #  #\n     #  #\n    ##  #\n    #   #\n   #G R #\n   ######',
    'puzznic24': '###########\n#RBP     R#\n####     ##\n   ###P#B# \n    ###### ',
    'puzznic25': '##########\n#L      L#\n#O      O#\n##      ##\n##      ##\n## #### ##\n##########',
    'puzznic26': '  ######  \n #  B L # \n#   R G  #\n#   G P  #\n# R R L  #\n# P P B  #\n##########',
    'puzznic27': '##########\n#       Y#\n#      YP#\n#   L  PR#\n#CG GP GY#\n#GL CBPBR#\n##########',
    'puzznic28': '##########\n# O#L O###\n# ##B B###\n#G#GO ####\n#BYRG L###\n#OG#PB#LR#\n#VYVBGLPB#\n##########',
    'puzznic34': '##########\n####### Y#\n####### ##\n#####VP ##\n#### L# ##\n##   #  ##\n#V P RYRL#\n##########',
    'puzznic35': ' ######## \n##      ##\n#G      B#\n## BRBPG##\n # ###### \n #    #   \n #### ##  \n  #    #  \n  #P#  #  \n  ###R##  \n   ####',
    'puzznic36': '  ######  \n  #O  O#  \n####  ####\n# L    Y #\n#CPR  BPC#\n####  ####\n# O    OY#\n#OGB  BPG#\n####  ####\n  ##PR##  \n  ##RL##  \n  ######',
    'puzznic37': '##########\n#        #\n#        #\n#        #\n#        #\n#        #\n#        #\n# B BL   #\n# R GR   #\n# BGRLGP #\n# RBGRPB #\n##########',
    'puzznic38': '##########\n#RB R    #\n#BR P    #\n#######  #\n    #### #\n    # PB #\n    # ####\n##### ####\n##B## #P##\n#BPBPBPBP#\n##BPBPBP##\n##########',
    'puzznic39': '##########\n#B   L   #\n#PR###   #\n #G  #GY# \n  #  OR#  \n   # ##   \n   #R #   \n  # G R#  \n #G O#L # \n# L#R#C L#\n#LY#L#PCB#\n##########',
    'puzznic40': '##########\n#        #\n#  CYP   #\n#  G#B   #\n#  ###   #\n#Y O  PGP#\n#C#G  R#G#\n####  ####\n#   BPB  #\n#   O#R  #\n#   ###  #\n##########',
    'puzznic41': '##########\n#        #\n#     C  #\n#     R  #\n#    #C  #\n#     G  #\n#  #  Y  #\n#     GR #\n#     YLB#\n##L#  RPL#\n##R#  PLB#\n##########',
    'puzznic42': '##########\n# B      #\n##P R B#R#\n# # G P G#\n#   ### ##\n#RG      #\n### #### #\n#     P ##\n#P# #### #\n#R       #\n#GB BPRGB#\n##########',
    'puzznic43': '#####     \n#   #     \n#   #     \n#   #     \n#  B #####\n# CLP #  #\n# GRB##  #\n#R###    #\n#L# P    #\n#G ## GR #\n#C  P RG #\n##########',
    'puzznic44': '##########\n# BCR GL #\n# ### #B #\n# BLB ## #\n# ###G#  #\n# R OR   #\n# BYLY#  #\n#BRO####B#\n#POPR # C#\n#R#####B##\n#P P   #  \n########  ',
    'puzznic45': ' ######## \n#YL      #\n###Y     #\n####     #\n### G  R #\n#   ## P #\n#   ## B #\n#   #  # #\n#    R # #\n# LCGP # #\n# GLCB ###\n ######## '
}
