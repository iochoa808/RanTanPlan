"""
Shared generator for the 85 Plotting benchmark instances (grids up to 7x7,
compared against the published model in Espasa et al. 2024b -- the actual
paper benchmark).

Replicates the exact UP model in
~/unified-planning/docs/extensions/domains/plotting/Plotting.py
(generalized from its hardcoded example to arbitrary instance grid /
remaining_blocks): an rows x columns ArrayType(Colour) fluent 'blocks' plus
a scalar 'hand' fluent, 4 action schemas (shoot_partial_row, shoot_column,
shoot_row_and_column, shoot_only_full_row) using nested Forall/Exists over
RangeVariables and conditional (guarded) effects, and a goal expressed via
a Count() expression: at most `remaining_blocks` non-empty cells left.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))

from unified_planning.shortcuts import *


def build_plotting(name: str, instance, remaining_blocks: int):
    p = Problem(name)

    Colour = UserType('Colour')
    R = Object('R', Colour)
    B = Object('B', Colour)
    G = Object('G', Colour)
    Y = Object('Y', Colour)
    O = Object('O', Colour)
    V = Object('V', Colour)
    W = Object('W', Colour)  # wildcard (hand's initial value)
    N = Object('N', Colour)  # none / empty cell
    colour_by_symbol = {'R': R, 'B': B, 'G': G, 'Y': Y, 'O': O, 'V': V, 'W': W, 'N': N}
    p.add_objects([W, N])

    initial_blocks = []
    for row in instance:
        inside = []
        for ch in row:
            colour_obj = colour_by_symbol[ch]
            if not p.has_object(ch):
                p.add_object(colour_obj)
            inside.append(colour_obj)
        initial_blocks.append(inside)

    rows = len(initial_blocks)
    columns = len(initial_blocks[0])
    lr = rows - 1
    lc = columns - 1

    blocks = Fluent('blocks', ArrayType(rows, ArrayType(columns, Colour)))
    hand = Fluent('hand', Colour)
    p.add_fluent(blocks, default_initial_value=N)
    p.add_fluent(hand, default_initial_value=W)
    p.set_initial_value(blocks, initial_blocks)
    # RTP's native ingestion requires an explicit :init entry for every
    # object fluent -- default_initial_value alone isn't enough for a
    # plain (unparameterized) object fluent, same issue as labyrinth_ipc.py's
    # robot_at.
    p.set_initial_value(hand, W)

    shoot_partial_row = InstantaneousAction('shoot_partial_row', p_=Colour, r=IntType(0, rows - 1), l=IntType(0, columns - 2))
    p_, r, l = shoot_partial_row.parameter('p_'), shoot_partial_row.parameter('r'), shoot_partial_row.parameter('l')
    shoot_partial_row.add_precondition(Not(Equals(p_, N)))
    shoot_partial_row.add_precondition(Not(Equals(p_, W)))
    shoot_partial_row.add_precondition(Or(Equals(hand, p_), Equals(hand, W)))
    shoot_partial_row.add_precondition(And(
        Not(Equals(blocks[r][l + 1], p_)), Not(Equals(blocks[r][l + 1], N)), Not(Equals(blocks[r][l + 1], W))))
    b = RangeVariable('b', 0, l)
    shoot_partial_row.add_precondition(Forall(Or(Equals(blocks[r][b], p_), Equals(blocks[r][b], N)), b))
    shoot_partial_row.add_precondition(Exists(Equals(blocks[r][b], p_), b))
    shoot_partial_row.add_effect(hand, blocks[r][l + 1])
    shoot_partial_row.add_effect(blocks[r][l + 1], p_)
    shoot_partial_row.add_effect(blocks[0][b], N, forall=[b])
    a = RangeVariable('a', 1, r)
    shoot_partial_row.add_effect(blocks[a][b], blocks[a - 1][b], forall=[a, b])
    p.add_action(shoot_partial_row)

    # l=IntType(0, rows-1), not IntType(0, rows) as in the original
    # Plotting.py -- that's an off-by-one (l represents a landing row
    # index, valid range [0, rows-1]; the "shot reaches the bottom" case
    # is already handled by the l==lr check below, not by l==rows). The
    # original value makes RangeVariable('b', 0, l) reach index `rows`,
    # one past the array bound -- confirmed by running the original
    # script unmodified: it fails with the identical UPTypeError.
    shoot_column = InstantaneousAction('shoot_column', p_=Colour, c=IntType(0, columns - 1), l=IntType(0, rows - 1))
    p_, c, l = shoot_column.parameter('p_'), shoot_column.parameter('c'), shoot_column.parameter('l')
    shoot_column.add_precondition(Not(Equals(p_, N)))
    shoot_column.add_precondition(Not(Equals(p_, W)))
    shoot_column.add_precondition(Or(Equals(hand, p_), Equals(hand, W)))
    shoot_column.add_precondition(Or(Equals(l, lr), And(
        Not(Equals(blocks[l + 1][c], p_)), Not(Equals(blocks[l + 1][c], N)), Not(Equals(blocks[l + 1][c], W)))))
    b = RangeVariable('b', 0, l)
    shoot_column.add_precondition(Forall(Or(Equals(blocks[b][c], p_), Equals(blocks[b][c], N)), b))
    shoot_column.add_precondition(Exists(Equals(blocks[b][c], p_), b))
    shoot_column.add_effect(hand, blocks[l + 1][c], LT(l, lr))
    shoot_column.add_effect(hand, p_, Equals(l, lr))
    shoot_column.add_effect(blocks[l + 1][c], p_, LT(l, lr))
    shoot_column.add_effect(blocks[b][c], N, forall=[b])
    p.add_action(shoot_column)

    shoot_row_and_column = InstantaneousAction('shoot_row_and_column', p_=Colour, r=IntType(0, rows - 2), l=IntType(1, rows - 1))
    p_, r, l = shoot_row_and_column.parameter('p_'), shoot_row_and_column.parameter('r'), shoot_row_and_column.parameter('l')
    shoot_row_and_column.add_precondition(GT(l, r))
    shoot_row_and_column.add_precondition(Not(Equals(p_, N)))
    shoot_row_and_column.add_precondition(Not(Equals(p_, W)))
    shoot_row_and_column.add_precondition(Or(Equals(hand, p_), Equals(hand, W)))
    c = RangeVariable('c', 0, lc)
    shoot_row_and_column.add_precondition(Forall(Or(Equals(blocks[r][c], p_), Equals(blocks[r][c], N)), c))
    b = RangeVariable('b', r + 1, l)
    shoot_row_and_column.add_precondition(Forall(Or(Equals(blocks[b][lc], p_), Equals(blocks[b][lc], N)), b))
    shoot_row_and_column.add_precondition(Or(
        Exists(Equals(blocks[r][c], p_), c),
        Exists(Equals(blocks[b][lc], p_), b)
    ))
    shoot_row_and_column.add_precondition(Or(Equals(l, lr), And(Not(Equals(blocks[l + 1][lc], p_)),
                                                                 Not(Equals(blocks[l + 1][lc], N)))))
    shoot_row_and_column.add_effect(blocks[l + 1][lc], p_, LT(l, lr))
    shoot_row_and_column.add_effect(hand, blocks[l + 1][lc], LT(l, lr))
    shoot_row_and_column.add_effect(hand, p_, Equals(l, lr))
    a = RangeVariable('a', 1, r)
    c = RangeVariable('c', 0, lc - 1)
    shoot_row_and_column.add_effect(blocks[0][c], N, forall=[c])
    shoot_row_and_column.add_effect(blocks[a][c], blocks[a - 1][c], forall=[a, c])
    b = RangeVariable('b', 0, r - 1)
    shoot_row_and_column.add_effect(blocks[l - b][lc], blocks[b][lc], forall=[b])
    x = RangeVariable('x', 0, l - r)
    shoot_row_and_column.add_effect(blocks[x][lc], N, forall=[x])
    p.add_action(shoot_row_and_column)

    shoot_only_full_row = InstantaneousAction('shoot_only_full_row', p_=Colour, r=IntType(0, rows - 1))
    p_, r = shoot_only_full_row.parameter('p_'), shoot_only_full_row.parameter('r')
    shoot_only_full_row.add_precondition(Not(Equals(p_, N)))
    shoot_only_full_row.add_precondition(Not(Equals(p_, W)))
    shoot_only_full_row.add_precondition(Or(Equals(hand, p_), Equals(hand, W)))
    c = RangeVariable('c', 0, lc)
    shoot_only_full_row.add_precondition(Forall(Or(Equals(blocks[r][c], p_), Equals(blocks[r][c], N)), c))
    shoot_only_full_row.add_precondition(Exists(Equals(blocks[r][c], p_), c))
    shoot_only_full_row.add_precondition(Or(Equals(r, lr), And(
        Not(Equals(blocks[r + 1][lc], p_)), Not(Equals(blocks[r + 1][lc], N)), Not(Equals(blocks[r + 1][lc], W))
    )))
    shoot_only_full_row.add_effect(blocks[r + 1][lc], p_, LT(r, lr))
    shoot_only_full_row.add_effect(hand, blocks[r + 1][lc], LT(r, lr))
    shoot_only_full_row.add_effect(hand, p_, Equals(r, lr))
    a = RangeVariable('a', 1, r)
    c = RangeVariable('c', 0, lc)
    shoot_only_full_row.add_effect(blocks[0][c], N, forall=[c])
    shoot_only_full_row.add_effect(blocks[a][c], blocks[a - 1][c], forall=[a, c])
    p.add_action(shoot_only_full_row)

    rb = [Not(Equals(blocks[i][j], N)) for i in range(rows) for j in range(columns)]
    p.add_goal(LE(Count(rb), remaining_blocks))

    costs = {
        shoot_partial_row: Int(1), shoot_column: Int(1),
        shoot_only_full_row: Int(1), shoot_row_and_column: Int(1),
    }
    p.add_quality_metric(MinimizeActionCosts(costs))
    return p


INSTANCES = {
    'plt0_2_4_2_1': (['RRRG', 'RGGG'], 1),
    'plt0_2_4_2_2': (['RRRG', 'RGGG'], 2),
    'plt0_2_4_2_4': (['RRRG', 'RGGG'], 4),
    'plt0_3_3_2_1': (['RRR', 'GGR', 'RRR'], 1),
    'plt0_3_3_2_2': (['RRR', 'GGR', 'RRR'], 2),
    'plt0_3_3_2_4': (['RRR', 'GGR', 'RRR'], 4),
    'plt0_3_3_3_2': (['RGG', 'BBG', 'GBR'], 2),
    'plt0_3_3_3_3': (['RGG', 'BBG', 'GBR'], 3),
    'plt0_3_3_3_4': (['RGG', 'BBG', 'GBR'], 4),
    'plt0_3_4_2_1': (['RRRG', 'RGRR', 'RGRG'], 1),
    'plt0_3_4_2_2': (['RRRG', 'RGRR', 'RGRG'], 2),
    'plt0_3_4_2_6': (['RRRG', 'RGRR', 'RGRG'], 6),
    'plt0_3_4_3_2': (['RGBB', 'GBRB', 'BGGR'], 2),
    'plt0_3_4_3_3': (['RGBB', 'GBRB', 'BGGR'], 3),
    'plt0_3_4_3_6': (['RGBB', 'GBRB', 'BGGR'], 6),
    'plt0_3_5_3_2': (['RGBBG', 'GGBRB', 'BGRBR'], 2),
    'plt0_3_5_3_3': (['RGBBG', 'GGBRB', 'BGRBR'], 3),
    'plt0_3_5_3_7': (['RGBBG', 'GGBRB', 'BGRBR'], 7),
    'plt0_3_5_4_3': (['RGGBY', 'YYYRB', 'YBYBY'], 3),
    'plt0_3_5_4_4': (['RGGBY', 'YYYRB', 'YBYBY'], 4),
    'plt0_3_5_4_7': (['RGGBY', 'YYYRB', 'YBYBY'], 7),
    'plt0_3_6_3_2': (['RGRGGR', 'RBGGGR', 'GRGRBR'], 2),
    'plt0_3_6_3_3': (['RGRGGR', 'RBGGGR', 'GRGRBR'], 3),
    'plt0_3_6_3_9': (['RGRGGR', 'RBGGGR', 'GRGRBR'], 9),
    'plt0_3_6_4_3': (['RGGGRB', 'RGRYBR', 'YYRYYR'], 3),
    'plt0_3_6_4_4': (['RGGGRB', 'RGRYBR', 'YYRYYR'], 4),
    'plt0_3_6_4_9': (['RGGGRB', 'RGRYBR', 'YYRYYR'], 9),
    'plt0_4_2_2_1': (['RG', 'GR', 'RG', 'GG'], 1),
    'plt0_4_2_2_2': (['RG', 'GR', 'RG', 'GG'], 2),
    'plt0_4_2_2_4': (['RG', 'GR', 'RG', 'GG'], 4),
    'plt0_4_3_2_1': (['RGR', 'GGG', 'RRR', 'RGR'], 1),
    'plt0_4_3_2_2': (['RGR', 'GGG', 'RRR', 'RGR'], 2),
    'plt0_4_3_2_6': (['RGR', 'GGG', 'RRR', 'RGR'], 6),
    'plt0_4_3_3_2': (['RRG', 'BGR', 'GGR', 'RRG'], 2),
    'plt0_4_3_3_3': (['RRG', 'BGR', 'GGR', 'RRG'], 3),
    'plt0_4_3_3_6': (['RRG', 'BGR', 'GGR', 'RRG'], 6),
    'plt0_4_4_3_2': (['RRRR', 'RRRR', 'RRRR', 'RGGB'], 2),
    'plt0_4_4_3_3': (['RRRR', 'RRRR', 'RRRR', 'RGGB'], 3),
    'plt0_4_4_3_8': (['RRRR', 'RRRR', 'RRRR', 'RGGB'], 8),
    'plt0_4_4_4_3': (['RGRB', 'YGBY', 'GYBG', 'BGBG'], 3),
    'plt0_4_4_4_4': (['RGRB', 'YGBY', 'GYBG', 'BGBG'], 4),
    'plt0_4_4_4_8': (['RGRB', 'YGBY', 'GYBG', 'BGBG'], 8),
    'plt0_4_5_3_10': (['RRRRR', 'GRGRB', 'RGBBB', 'GRGRG'], 10),
    'plt0_4_5_3_2': (['RRRRR', 'GRGRB', 'RGBBB', 'GRGRG'], 2),
    'plt0_4_5_3_3': (['RRRRR', 'GRGRB', 'RGBBB', 'GRGRG'], 3),
    'plt0_4_5_4_10': (['RRRRR', 'RRRRR', 'RGBYR', 'YRGRR'], 10),
    'plt0_4_5_4_3': (['RRRRR', 'RRRRR', 'RGBYR', 'YRGRR'], 3),
    'plt0_4_5_4_4': (['RRRRR', 'RRRRR', 'RGBYR', 'YRGRR'], 4),
    'plt0_5_3_3_2': (['RGB', 'RBB', 'BBR', 'GGR', 'BGR'], 2),
    'plt0_5_3_3_3': (['RGB', 'RBB', 'BBR', 'GGR', 'BGR'], 3),
    'plt0_5_3_3_7': (['RGB', 'RBB', 'BBR', 'GGR', 'BGR'], 7),
    'plt0_5_3_4_3': (['RGR', 'RGG', 'BYB', 'GYY', 'YGY'], 3),
    'plt0_5_3_4_4': (['RGR', 'RGG', 'BYB', 'GYY', 'YGY'], 4),
    'plt0_5_3_4_7': (['RGR', 'RGG', 'BYB', 'GYY', 'YGY'], 7),
    'plt0_5_4_3_10': (['RRRR', 'RRGR', 'BGBR', 'GBGB', 'RRRR'], 10),
    'plt0_5_4_3_2': (['RRRR', 'RRGR', 'BGBR', 'GBGB', 'RRRR'], 2),
    'plt0_5_4_3_3': (['RRRR', 'RRGR', 'BGBR', 'GBGB', 'RRRR'], 3),
    'plt0_5_4_4_10': (['RRGR', 'BGYY', 'BBBG', 'GGGY', 'GBRB'], 10),
    'plt0_5_4_4_3': (['RRGR', 'BGYY', 'BBBG', 'GGGY', 'GBRB'], 3),
    'plt0_5_4_4_4': (['RRGR', 'BGYY', 'BBBG', 'GGGY', 'GBRB'], 4),
    'plt0_5_5_3_12': (['RRGRR', 'RBGRG', 'GBGRR', 'BBGRG', 'GGGRB'], 12),
    'plt0_5_5_3_2': (['RRGRR', 'RBGRG', 'GBGRR', 'BBGRG', 'GGGRB'], 2),
    'plt0_5_5_3_3': (['RRGRR', 'RBGRG', 'GBGRR', 'BBGRG', 'GGGRB'], 3),
    'plt0_5_5_4_12': (['RRRRR', 'RRRRR', 'RRRRR', 'GBGYY', 'YRGRR'], 12),
    'plt0_5_5_4_3': (['RRRRR', 'RRRRR', 'RRRRR', 'GBGYY', 'YRGRR'], 3),
    'plt0_5_5_4_4': (['RRRRR', 'RRRRR', 'RRRRR', 'GBGYY', 'YRGRR'], 4),
    'plt0_5_6_3_15': (['RRRGRR', 'BRRBRB', 'RGBBGG', 'RRGBGB', 'GBRGBB'], 15),
    'plt0_5_6_3_2': (['RRRGRR', 'BRRBRB', 'RGBBGG', 'RRGBGB', 'GBRGBB'], 2),
    'plt0_5_6_3_3': (['RRRGRR', 'BRRBRB', 'RGBBGG', 'RRGBGB', 'GBRGBB'], 3),
    'plt0_5_6_5_15': (['RGBGBG', 'YOYOYO', 'GGBOOG', 'RORGYY', 'YBORYB'], 15),
    'plt0_5_6_5_4': (['RGBGBG', 'YOYOYO', 'GGBOOG', 'RORGYY', 'YBORYB'], 4),
    'plt0_5_6_5_5': (['RGBGBG', 'YOYOYO', 'GGBOOG', 'RORGYY', 'YBORYB'], 5),
    'plt0_6_3_3_2': (['RRR', 'GGR', 'BGG', 'BBB', 'GRB', 'RBR'], 2),
    'plt0_6_3_3_3': (['RRR', 'GGR', 'BGG', 'BBB', 'GRB', 'RBR'], 3),
    'plt0_6_3_3_9': (['RRR', 'GGR', 'BGG', 'BBB', 'GRB', 'RBR'], 9),
    'plt0_6_3_4_3': (['RRR', 'GRG', 'RRG', 'RRR', 'BRB', 'BGY'], 3),
    'plt0_6_3_4_4': (['RRR', 'GRG', 'RRG', 'RRR', 'BRB', 'BGY'], 4),
    'plt0_6_3_4_9': (['RRR', 'GRG', 'RRG', 'RRR', 'BRB', 'BGY'], 9),
    'plt0_6_5_3_15': (['RRGRR', 'RGGRR', 'RRGBR', 'BBBBG', 'BGBGB', 'GBBBR'], 15),
    'plt0_6_5_3_2': (['RRGRR', 'RGGRR', 'RRGBR', 'BBBBG', 'BGBGB', 'GBBBR'], 2),
    'plt0_6_5_3_3': (['RRGRR', 'RGGRR', 'RRGBR', 'BBBBG', 'BGBGB', 'GBBBR'], 3),
    'plt0_6_5_5_15': (['RGBBY', 'BBGRB', 'GYYBR', 'YRBYG', 'GBBOY', 'OGGRY'], 15),
    'plt0_6_5_5_4': (['RGBBY', 'BBGRB', 'GYYBR', 'YRBYG', 'GBBOY', 'OGGRY'], 4),
    'plt0_6_5_5_5': (['RGBBY', 'BBGRB', 'GYYBR', 'YRBYG', 'GBBOY', 'OGGRY'], 5),
    'plt0_7_7_6_24': (['RRGGBRR', 'YOVOYYR', 'BVBRROR', 'BBGBRGO', 'YYOVVGG', 'OROBRVG', 'RBYOOBR'], 24),
    'plt0_7_7_6_5': (['RRGGBRR', 'YOVOYYR', 'BVBRROR', 'BBGBRGO', 'YYOVVGG', 'OROBRVG', 'RBYOOBR'], 5),
    'plt0_7_7_6_6': (['RRGGBRR', 'YOVOYYR', 'BVBRROR', 'BBGBRGO', 'YYOVVGG', 'OROBRVG', 'RBYOOBR'], 6)
}
