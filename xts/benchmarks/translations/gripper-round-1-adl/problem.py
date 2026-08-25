"""
gripper-round-1-adl XTS. Object fluent: (robby-at) - room.
4 balls in rooma, move to roomb. Expected: 11 steps.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem("gripper_x_1_xts")

    Room    = UserType("room")
    Ball    = UserType("ball")
    Gripper = UserType("gripper")

    rooma = Object("rooma", Room)
    roomb = Object("roomb", Room)
    ball1 = Object("ball1", Ball)
    ball2 = Object("ball2", Ball)
    ball3 = Object("ball3", Ball)
    ball4 = Object("ball4", Ball)
    left  = Object("left",  Gripper)
    right = Object("right", Gripper)
    p.add_objects([rooma, roomb, ball1, ball2, ball3, ball4, left, right])

    # Boolean predicates
    at    = Fluent("at",    b=Ball, r=Room)
    carry = Fluent("carry", b=Ball, g=Gripper)
    free  = Fluent("free",  g=Gripper)
    p.add_fluent(at,    default_initial_value=False)
    p.add_fluent(carry, default_initial_value=False)
    p.add_fluent(free,  default_initial_value=False)

    # Object fluent: (robby-at) - room
    robby_at = Fluent("robby-at", Room)
    p.add_fluent(robby_at)

    # Initial state
    p.set_initial_value(robby_at, rooma)
    p.set_initial_value(free(left),  True)
    p.set_initial_value(free(right), True)
    p.set_initial_value(at(ball1, rooma), True)
    p.set_initial_value(at(ball2, rooma), True)
    p.set_initial_value(at(ball3, rooma), True)
    p.set_initial_value(at(ball4, rooma), True)

    # move(?from, ?to)
    move = InstantaneousAction("move", frm=Room, to=Room)
    frm_p, to_p = move.parameter("frm"), move.parameter("to")
    move.add_precondition(Equals(robby_at, frm_p))
    move.add_effect(robby_at, to_p)
    p.add_action(move)

    # pick(?obj, ?room, ?gripper)
    pick = InstantaneousAction("pick", obj=Ball, room=Room, gripper=Gripper)
    obj_p, room_p, gripper_p = pick.parameter("obj"), pick.parameter("room"), pick.parameter("gripper")
    pick.add_precondition(at(obj_p, room_p))
    pick.add_precondition(Equals(robby_at, room_p))
    pick.add_precondition(free(gripper_p))
    pick.add_effect(carry(obj_p, gripper_p), True)
    pick.add_effect(at(obj_p, room_p), False)
    pick.add_effect(free(gripper_p), False)
    p.add_action(pick)

    # drop(?obj, ?room, ?gripper)
    drop = InstantaneousAction("drop", obj=Ball, room=Room, gripper=Gripper)
    obj_p, room_p, gripper_p = drop.parameter("obj"), drop.parameter("room"), drop.parameter("gripper")
    drop.add_precondition(carry(obj_p, gripper_p))
    drop.add_precondition(Equals(robby_at, room_p))
    drop.add_effect(at(obj_p, room_p), True)
    drop.add_effect(carry(obj_p, gripper_p), False)
    drop.add_effect(free(gripper_p), True)
    p.add_action(drop)

    # Goal: all balls in roomb
    p.add_goal(at(ball1, roomb))
    p.add_goal(at(ball2, roomb))
    p.add_goal(at(ball3, roomb))
    p.add_goal(at(ball4, roomb))

    return p
