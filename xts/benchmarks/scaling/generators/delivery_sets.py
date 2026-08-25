"""
Delivery generator — XTS set-partition encoding.

N items in room-a, goal: all items in room-b. 2 rooms, robot carries 1 item at a time.
XTS encoding: (items-at ?r) set fluent per room; (carrying) set fluent for in-transit items.
Goal is (= (cardinality (items-at roomb)) N) — one atom.

Compare against: delivery_bool (boolean encoding) to see encoding impact on:
  - Compile overhead: SETS_REMOVING needed vs none for classic.
  - Solve time: diffViewpoints README notes pfile02 is ~15× SLOWER than boolean
    encoding — the set partition makes Z3's array theory work harder for mutable sets
    (items enter AND leave room sets). This is the counter-intuitive direction.
  - This demonstrates that more expressive encodings can hurt solve performance:
    the set model lifts the partition invariant into the formula structure, which Z3
    must reason about at every horizon step.

Pattern: cross-set transfer (P3) with carrying as an intermediate set.
pick:  remove(i, items-at(r)), add(i, carrying)
drop:  remove(i, carrying), add(i, items-at(r))
"""
import sys, os
sys.path.insert(0, os.path.expanduser('~/unified-planning'))
from unified_planning.shortcuts import *


def generate(n: int):
    """Return a delivery UP Problem with n items and set partition encoding."""
    p = Problem(f'delivery_sets_n{n}')
    Item    = UserType('item')
    Room    = UserType('room')
    ItemSet = SetType(Item)

    items  = [Object(f'item{i}', Item) for i in range(n)]
    room_a = Object('rooma', Room)
    room_b = Object('roomb', Room)
    rooms  = [room_a, room_b]
    p.add_objects(items + rooms)

    robot_at  = Fluent('robot_at',  Room)
    items_at  = Fluent('items_at',  ItemSet, r=Room)
    carrying  = Fluent('carrying',  ItemSet)
    hand_free = Fluent('hand_free', BoolType())
    p.add_fluent(robot_at)
    p.add_fluent(items_at)
    p.add_fluent(carrying)
    p.add_fluent(hand_free, default_initial_value=False)

    p.set_initial_value(robot_at, room_a)
    p.set_initial_value(items_at(room_a), set(items))
    p.set_initial_value(items_at(room_b), set())
    p.set_initial_value(carrying, set())
    p.set_initial_value(hand_free, True)

    move = InstantaneousAction('move', f=Room, t=Room)
    f, t = move.parameter('f'), move.parameter('t')
    move.add_precondition(Equals(robot_at, f))
    move.add_effect(robot_at, t)
    p.add_action(move)

    pick = InstantaneousAction('pick', i=Item, r=Room)
    i, r = pick.parameter('i'), pick.parameter('r')
    pick.add_precondition(Equals(robot_at, r))
    pick.add_precondition(SetMember(i, items_at(r)))
    pick.add_precondition(hand_free)
    pick.add_effect(items_at(r),  SetRemove(i, items_at(r)))
    pick.add_effect(carrying,     SetAdd(i, FluentExp(carrying)))
    pick.add_effect(hand_free,    False)
    p.add_action(pick)

    drop = InstantaneousAction('drop', i=Item, r=Room)
    i, r = drop.parameter('i'), drop.parameter('r')
    drop.add_precondition(Equals(robot_at, r))
    drop.add_precondition(SetMember(i, carrying))
    drop.add_effect(carrying,     SetRemove(i, FluentExp(carrying)))
    drop.add_effect(items_at(r),  SetAdd(i, items_at(r)))
    drop.add_effect(hand_free,    True)
    p.add_action(drop)

    # Goal: 1 cardinality atom — constant regardless of N
    p.add_goal(Equals(SetCardinality(items_at(room_b)), n))
    return p
