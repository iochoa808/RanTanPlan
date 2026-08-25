"""
Sokoban XTS — mini-sokoban 3x3.
Features: 2D array stones, bounded int player position.
Init: stones[1][1]=1, player at (0,0). Goal: stones[2][2]=1.
Plan: ~5 steps.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('mini-sokoban-xts')

    size_t = IntType(0, 2)
    cell_t = IntType(0, 1)

    stones = Fluent('stones', ArrayType(3, ArrayType(3, cell_t)))
    prow   = Fluent('prow',   size_t)
    pcol   = Fluent('pcol',   size_t)

    p.add_fluent(stones)
    p.add_fluent(prow, default_initial_value=Int(0))
    p.add_fluent(pcol, default_initial_value=Int(0))

    # Init: stone at (1,1), player at (0,0)
    p.set_initial_value(stones, [[0, 0, 0],
                                  [0, 1, 0],
                                  [0, 0, 0]])
    p.set_initial_value(prow, Int(0))
    p.set_initial_value(pcol, Int(0))

    # move-right: player moves row+1 (empty cell)
    move_right = InstantaneousAction('move-right', pr=size_t, pc=size_t)
    pr, pc = move_right.parameter('pr'), move_right.parameter('pc')
    move_right.add_precondition(Equals(prow, pr))
    move_right.add_precondition(Equals(pcol, pc))
    move_right.add_precondition(Equals(stones[Plus(pr, Int(1))][pc], Int(0)))
    move_right.add_effect(prow, Plus(pr, Int(1)))
    p.add_action(move_right)

    # move-left: player moves row-1 (empty cell)
    move_left = InstantaneousAction('move-left', pr=size_t, pc=size_t)
    pr, pc = move_left.parameter('pr'), move_left.parameter('pc')
    move_left.add_precondition(Equals(prow, pr))
    move_left.add_precondition(Equals(pcol, pc))
    move_left.add_precondition(Equals(stones[Minus(pr, Int(1))][pc], Int(0)))
    move_left.add_effect(prow, Minus(pr, Int(1)))
    p.add_action(move_left)

    # move-down: player moves col+1 (empty cell)
    move_down = InstantaneousAction('move-down', pr=size_t, pc=size_t)
    pr, pc = move_down.parameter('pr'), move_down.parameter('pc')
    move_down.add_precondition(Equals(prow, pr))
    move_down.add_precondition(Equals(pcol, pc))
    move_down.add_precondition(Equals(stones[pr][Plus(pc, Int(1))], Int(0)))
    move_down.add_effect(pcol, Plus(pc, Int(1)))
    p.add_action(move_down)

    # move-up: player moves col-1 (empty cell)
    move_up = InstantaneousAction('move-up', pr=size_t, pc=size_t)
    pr, pc = move_up.parameter('pr'), move_up.parameter('pc')
    move_up.add_precondition(Equals(prow, pr))
    move_up.add_precondition(Equals(pcol, pc))
    move_up.add_precondition(Equals(stones[pr][Minus(pc, Int(1))], Int(0)))
    move_up.add_effect(pcol, Minus(pc, Int(1)))
    p.add_action(move_up)

    # push-right: player at (pr,pc), stone at (pr+1,pc), empty at (pr+2,pc)
    push_right = InstantaneousAction('push-right', pr=size_t, pc=size_t)
    pr, pc = push_right.parameter('pr'), push_right.parameter('pc')
    push_right.add_precondition(Equals(prow, pr))
    push_right.add_precondition(Equals(pcol, pc))
    push_right.add_precondition(Equals(stones[Plus(pr, Int(1))][pc], Int(1)))
    push_right.add_precondition(Equals(stones[Plus(pr, Int(2))][pc], Int(0)))
    push_right.add_effect(stones[Plus(pr, Int(1))][pc], Int(0))
    push_right.add_effect(stones[Plus(pr, Int(2))][pc], Int(1))
    push_right.add_effect(prow, Plus(pr, Int(1)))
    p.add_action(push_right)

    # push-left: player at (pr,pc), stone at (pr-1,pc), empty at (pr-2,pc)
    push_left = InstantaneousAction('push-left', pr=size_t, pc=size_t)
    pr, pc = push_left.parameter('pr'), push_left.parameter('pc')
    push_left.add_precondition(Equals(prow, pr))
    push_left.add_precondition(Equals(pcol, pc))
    push_left.add_precondition(Equals(stones[Minus(pr, Int(1))][pc], Int(1)))
    push_left.add_precondition(Equals(stones[Minus(pr, Int(2))][pc], Int(0)))
    push_left.add_effect(stones[Minus(pr, Int(1))][pc], Int(0))
    push_left.add_effect(stones[Minus(pr, Int(2))][pc], Int(1))
    push_left.add_effect(prow, Minus(pr, Int(1)))
    p.add_action(push_left)

    # push-down: player at (pr,pc), stone at (pr,pc+1), empty at (pr,pc+2)
    push_down = InstantaneousAction('push-down', pr=size_t, pc=size_t)
    pr, pc = push_down.parameter('pr'), push_down.parameter('pc')
    push_down.add_precondition(Equals(prow, pr))
    push_down.add_precondition(Equals(pcol, pc))
    push_down.add_precondition(Equals(stones[pr][Plus(pc, Int(1))], Int(1)))
    push_down.add_precondition(Equals(stones[pr][Plus(pc, Int(2))], Int(0)))
    push_down.add_effect(stones[pr][Plus(pc, Int(1))], Int(0))
    push_down.add_effect(stones[pr][Plus(pc, Int(2))], Int(1))
    push_down.add_effect(pcol, Plus(pc, Int(1)))
    p.add_action(push_down)

    # push-up: player at (pr,pc), stone at (pr,pc-1), empty at (pr,pc-2)
    push_up = InstantaneousAction('push-up', pr=size_t, pc=size_t)
    pr, pc = push_up.parameter('pr'), push_up.parameter('pc')
    push_up.add_precondition(Equals(prow, pr))
    push_up.add_precondition(Equals(pcol, pc))
    push_up.add_precondition(Equals(stones[pr][Minus(pc, Int(1))], Int(1)))
    push_up.add_precondition(Equals(stones[pr][Minus(pc, Int(2))], Int(0)))
    push_up.add_effect(stones[pr][Minus(pc, Int(1))], Int(0))
    push_up.add_effect(stones[pr][Minus(pc, Int(2))], Int(1))
    push_up.add_effect(pcol, Minus(pc, Int(1)))
    p.add_action(push_up)

    # Goal: stone at (2,2)
    p.add_goal(Equals(stones[Int(2)][Int(2)], Int(1)))

    return p
