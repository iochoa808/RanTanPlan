"""
block-grouping: 3 blocks on 5x5 grid.
b1=(1,3), b2=(2,4), b3=(4,3).
Goal: b1 and b2 at same cell, NOT same as b3.
Optimal: move_block_right(b1) -> (2,3), move_block_down(b2) -> (2,3). 2 steps.
PDDL-XTS: block-grouping
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem("block-grouping-3-xts")

    Block  = UserType("block")
    coord_t = IntType(1, 5)

    x = Fluent("x", coord_t, b=Block)
    y = Fluent("y", coord_t, b=Block)

    p.add_fluent(x, default_initial_value=1)
    p.add_fluent(y, default_initial_value=1)

    b1 = Object("b1", Block)
    b2 = Object("b2", Block)
    b3 = Object("b3", Block)
    p.add_objects([b1, b2, b3])

    p.set_initial_value(x(b1), Int(1))
    p.set_initial_value(y(b1), Int(3))
    p.set_initial_value(x(b2), Int(2))
    p.set_initial_value(y(b2), Int(4))
    p.set_initial_value(x(b3), Int(4))
    p.set_initial_value(y(b3), Int(3))

    move_up = InstantaneousAction("move_block_up", b=Block)
    b = move_up.parameter("b")
    move_up.add_precondition(LT(y(b), Int(5)))
    move_up.add_effect(y(b), Plus(y(b), Int(1)))
    p.add_action(move_up)

    move_down = InstantaneousAction("move_block_down", b=Block)
    b = move_down.parameter("b")
    move_down.add_precondition(GT(y(b), Int(1)))
    move_down.add_effect(y(b), Minus(y(b), Int(1)))
    p.add_action(move_down)

    move_right = InstantaneousAction("move_block_right", b=Block)
    b = move_right.parameter("b")
    move_right.add_precondition(LT(x(b), Int(5)))
    move_right.add_effect(x(b), Plus(x(b), Int(1)))
    p.add_action(move_right)

    move_left = InstantaneousAction("move_block_left", b=Block)
    b = move_left.parameter("b")
    move_left.add_precondition(GT(x(b), Int(1)))
    move_left.add_effect(x(b), Minus(x(b), Int(1)))
    p.add_action(move_left)

    # Goal: x(b1)==x(b2) AND y(b1)==y(b2) AND (x(b1)!=x(b3) OR y(b1)!=y(b3))
    p.add_goal(Equals(x(b1), x(b2)))
    p.add_goal(Equals(y(b1), y(b2)))
    p.add_goal(Or(Not(Equals(x(b1), x(b3))), Not(Equals(y(b1), y(b3)))))
    return p
