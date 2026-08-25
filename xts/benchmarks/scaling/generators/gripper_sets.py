"""
Gripper generator — XTS set-partition encoding.

N balls in room-a, goal: all balls in room-b. 2 rooms, 2 grippers.
XTS encoding: (balls-at ?r) and (carried ?g) set fluents;
goal is (= (cardinality (balls-at roomb)) N) — ONE atom regardless of N.

Compare against: gripper_bool (boolean-predicate encoding) to see:
  - Compile overhead: SETS_REMOVING needed vs none for classic.
  - Solve difference: 1 cardinality goal vs N boolean atoms.
  - State size: 4 set fluents (constant) vs N×2 boolean predicates (linear in N).

Pattern: cross-set transfer (P3). pick removes from balls-at(r), adds to carried(g);
drop removes from carried(g), adds to balls-at(r). Each fluent touched once per action.
(free ?g) kept as a boolean predicate — avoids cardinality-in-precondition.
"""
import sys, os
sys.path.insert(0, os.path.expanduser('~/unified-planning'))
from unified_planning.shortcuts import *


def generate(n: int):
    """Return a gripper UP Problem with n balls and set partition encoding."""
    p = Problem(f'gripper_sets_n{n}')
    Ball    = UserType('ball')
    Room    = UserType('room')
    Gripper = UserType('gripper')
    BallSet = SetType(Ball)

    balls    = [Object(f'ball{i}', Ball) for i in range(n)]
    room_a   = Object('rooma', Room)
    room_b   = Object('roomb', Room)
    rooms    = [room_a, room_b]
    grippers = [Object('left', Gripper), Object('right', Gripper)]
    p.add_objects(balls + rooms + grippers)

    robby_at = Fluent('robby_at', Room)
    balls_at = Fluent('balls_at', BallSet, r=Room)
    carried  = Fluent('carried',  BallSet, g=Gripper)
    free     = Fluent('free',     BoolType(), g=Gripper)
    p.add_fluent(robby_at)
    p.add_fluent(balls_at)
    p.add_fluent(carried)
    p.add_fluent(free, default_initial_value=False)

    p.set_initial_value(robby_at, room_a)
    p.set_initial_value(balls_at(room_a), set(balls))
    p.set_initial_value(balls_at(room_b), set())
    for g in grippers:
        p.set_initial_value(carried(g), set())
        p.set_initial_value(free(g), True)

    move = InstantaneousAction('move', f=Room, t=Room)
    f, t = move.parameter('f'), move.parameter('t')
    move.add_precondition(Equals(robby_at, f))
    move.add_effect(robby_at, t)
    p.add_action(move)

    pick = InstantaneousAction('pick', b=Ball, r=Room, g=Gripper)
    b, r, g = [pick.parameter(x) for x in ['b', 'r', 'g']]
    pick.add_precondition(Equals(robby_at, r))
    pick.add_precondition(SetMember(b, balls_at(r)))
    pick.add_precondition(free(g))
    pick.add_effect(balls_at(r), SetRemove(b, balls_at(r)))
    pick.add_effect(carried(g),  SetAdd(b, carried(g)))
    pick.add_effect(free(g), False)
    p.add_action(pick)

    drop = InstantaneousAction('drop', b=Ball, r=Room, g=Gripper)
    b, r, g = [drop.parameter(x) for x in ['b', 'r', 'g']]
    drop.add_precondition(Equals(robby_at, r))
    drop.add_precondition(SetMember(b, carried(g)))
    drop.add_effect(carried(g),  SetRemove(b, carried(g)))
    drop.add_effect(balls_at(r), SetAdd(b, balls_at(r)))
    drop.add_effect(free(g), True)
    p.add_action(drop)

    # Goal: 1 cardinality atom — constant regardless of N
    p.add_goal(Equals(SetCardinality(balls_at(room_b)), n))
    return p
