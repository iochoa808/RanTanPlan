"""
Delivery generator — classic boolean-predicate encoding.

N items in room-a, goal: all items in room-b. 2 rooms, robot carries 1 item at a time.
Classic encoding: (at ?i ?r) boolean predicate per item/room pair.
Goal has N atoms.

Compare against: delivery_sets (set partition encoding) to see the encoding impact on:
  - Compile overhead: none for classic vs SETS_REMOVING for XTS.
  - Solve time: N boolean atoms vs 1 cardinality goal.
  - State explosion: delivery with sets may be SLOWER than boolean due to Z3
    array-theory overhead for the bidirectional set (items enter AND leave rooms).
    This is the counter-intuitive case — more expressive encoding ≠ faster solve.

Min plan length: N × (pick + drop + 2 × move_between_rooms) — 4N steps.
"""
import sys, os
sys.path.insert(0, os.path.expanduser('~/unified-planning'))
from unified_planning.shortcuts import *


def generate(n: int):
    """Return a delivery UP Problem with n items and boolean predicates."""
    p = Problem(f'delivery_bool_n{n}')
    Item = UserType('item')
    Room = UserType('room')

    items  = [Object(f'item{i}', Item) for i in range(n)]
    room_a = Object('rooma', Room)
    room_b = Object('roomb', Room)
    rooms  = [room_a, room_b]
    p.add_objects(items + rooms)

    at_robot  = Fluent('at_robot',  BoolType(), r=Room)
    at        = Fluent('at',        BoolType(), i=Item, r=Room)
    carrying  = Fluent('carrying',  BoolType(), i=Item)
    hand_free = Fluent('hand_free', BoolType())
    p.add_fluent(at_robot,  default_initial_value=False)
    p.add_fluent(at,        default_initial_value=False)
    p.add_fluent(carrying,  default_initial_value=False)
    p.add_fluent(hand_free, default_initial_value=False)

    p.set_initial_value(at_robot(room_a), True)
    p.set_initial_value(hand_free, True)
    for i in items:
        p.set_initial_value(at(i, room_a), True)

    move = InstantaneousAction('move', f=Room, t=Room)
    f, t = move.parameter('f'), move.parameter('t')
    move.add_precondition(at_robot(f))
    move.add_effect(at_robot(t), True)
    move.add_effect(at_robot(f), False)
    p.add_action(move)

    pick = InstantaneousAction('pick', i=Item, r=Room)
    i, r = pick.parameter('i'), pick.parameter('r')
    pick.add_precondition(at_robot(r))
    pick.add_precondition(at(i, r))
    pick.add_precondition(hand_free)
    pick.add_effect(carrying(i),  True)
    pick.add_effect(at(i, r),     False)
    pick.add_effect(hand_free,    False)
    p.add_action(pick)

    drop = InstantaneousAction('drop', i=Item, r=Room)
    i, r = drop.parameter('i'), drop.parameter('r')
    drop.add_precondition(at_robot(r))
    drop.add_precondition(carrying(i))
    drop.add_effect(at(i, r),     True)
    drop.add_effect(carrying(i),  False)
    drop.add_effect(hand_free,    True)
    p.add_action(drop)

    # Goal: N boolean atoms — one per item ← grows linearly with N
    for i in items:
        p.add_goal(at(i, room_b))
    return p
