"""
Gripper generator — classic boolean-predicate encoding.

N balls in room-a, goal: all balls in room-b. 2 rooms, 2 grippers.
Classic encoding: (at ?b ?r) and (carry ?b ?g) boolean predicates per ball.
Goal has N atoms (one per ball).

Compare against: gripper_sets (set partition encoding) to see:
  - Compile overhead: classic needs no XTS transformation vs SETS_REMOVING cost.
  - Solve difference: N boolean goal atoms vs 1 set cardinality equality.
  - State size: classic tracks N×2 at-predicates + N×2 carry-predicates;
    sets use 4 set fluents regardless of N.

Min plan length: ceil(N/2) × 5 actions (2 balls per trip with 2 grippers).
"""
import sys, os
sys.path.insert(0, os.path.expanduser('~/unified-planning'))
from unified_planning.shortcuts import *


def generate(n: int):
    """Return a gripper UP Problem with n balls and boolean predicates."""
    p = Problem(f'gripper_bool_n{n}')
    Ball    = UserType('ball')
    Room    = UserType('room')
    Gripper = UserType('gripper')

    balls    = [Object(f'ball{i}', Ball) for i in range(n)]
    room_a   = Object('rooma', Room)
    room_b   = Object('roomb', Room)
    rooms    = [room_a, room_b]
    grippers = [Object('left', Gripper), Object('right', Gripper)]
    p.add_objects(balls + rooms + grippers)

    at_robby = Fluent('at_robby', BoolType(), r=Room)
    at       = Fluent('at',       BoolType(), b=Ball, r=Room)
    carry    = Fluent('carry',    BoolType(), b=Ball, g=Gripper)
    free     = Fluent('free',     BoolType(), g=Gripper)
    p.add_fluent(at_robby, default_initial_value=False)
    p.add_fluent(at,       default_initial_value=False)
    p.add_fluent(carry,    default_initial_value=False)
    p.add_fluent(free,     default_initial_value=False)

    p.set_initial_value(at_robby(room_a), True)
    for b in balls:
        p.set_initial_value(at(b, room_a), True)
    for g in grippers:
        p.set_initial_value(free(g), True)

    move = InstantaneousAction('move', f=Room, t=Room)
    f, t = move.parameter('f'), move.parameter('t')
    move.add_precondition(at_robby(f))
    move.add_effect(at_robby(t), True)
    move.add_effect(at_robby(f), False)
    p.add_action(move)

    pick = InstantaneousAction('pick', b=Ball, r=Room, g=Gripper)
    b, r, g = [pick.parameter(x) for x in ['b', 'r', 'g']]
    pick.add_precondition(at_robby(r))
    pick.add_precondition(at(b, r))
    pick.add_precondition(free(g))
    pick.add_effect(carry(b, g), True)
    pick.add_effect(at(b, r),    False)
    pick.add_effect(free(g),     False)
    p.add_action(pick)

    drop = InstantaneousAction('drop', b=Ball, r=Room, g=Gripper)
    b, r, g = [drop.parameter(x) for x in ['b', 'r', 'g']]
    drop.add_precondition(at_robby(r))
    drop.add_precondition(carry(b, g))
    drop.add_effect(at(b, r),    True)
    drop.add_effect(carry(b, g), False)
    drop.add_effect(free(g),     True)
    p.add_action(drop)

    # Goal: N boolean atoms — one per ball ← grows with N
    for b in balls:
        p.add_goal(at(b, room_b))
    return p
