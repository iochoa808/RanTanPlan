"""
Delivery XTS — 4 items delivered by 2 bots to two rooms.
XTS features: bounded integers (load), sets (doors), object fluents (bot-at).
Plan: 10 steps (pick x4, move x2, drop x4).
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('delivery_xts')

    Room = UserType('room')
    Item = UserType('item')
    Arm  = UserType('arm')
    Bot  = UserType('bot')

    load_t = IntType(0, 4)

    # Boolean predicates
    at      = Fluent('at', i=Item, x=Room)
    free    = Fluent('free', a=Arm)
    in_arm  = Fluent('in_arm', i=Item, a=Arm)
    in_tray = Fluent('in_tray', i=Item, b=Bot)
    mount   = Fluent('mount', a=Arm, b=Bot)
    for f in [at, free, in_arm, in_tray, mount]:
        p.add_fluent(f, default_initial_value=False)

    # Set fluent: (doors ?x) - roomset
    doors = Fluent('doors', SetType(Room), x=Room)
    p.add_fluent(doors, default_initial_value=set())

    # Object fluent: (bot-at ?b) - room
    bot_at = Fluent('bot_at', Room, b=Bot)
    p.add_fluent(bot_at)

    # Bounded int fluents
    current_load = Fluent('current_load', load_t, b=Bot)
    weight       = Fluent('weight', load_t, i=Item)
    load_limit   = Fluent('load_limit', load_t, b=Bot)
    p.add_fluent(current_load, default_initial_value=0)
    p.add_fluent(weight,       default_initial_value=0)
    p.add_fluent(load_limit,   default_initial_value=0)

    # Objects
    rooma  = Object('rooma', Room)
    roomb  = Object('roomb', Room)
    roomc  = Object('roomc', Room)
    item1  = Object('item1', Item)
    item2  = Object('item2', Item)
    item3  = Object('item3', Item)
    item4  = Object('item4', Item)
    bot1   = Object('bot1', Bot)
    bot2   = Object('bot2', Bot)
    left1  = Object('left1', Arm)
    right1 = Object('right1', Arm)
    left2  = Object('left2', Arm)
    right2 = Object('right2', Arm)
    p.add_objects([rooma, roomb, roomc,
                   item1, item2, item3, item4,
                   bot1, bot2,
                   left1, right1, left2, right2])

    # Initial state
    p.set_initial_value(bot_at(bot1), rooma)
    p.set_initial_value(bot_at(bot2), rooma)
    for arm in [left1, right1, left2, right2]:
        p.set_initial_value(free(arm), True)
    p.set_initial_value(mount(left1,  bot1), True)
    p.set_initial_value(mount(right1, bot1), True)
    p.set_initial_value(mount(left2,  bot2), True)
    p.set_initial_value(mount(right2, bot2), True)
    for item in [item1, item2, item3, item4]:
        p.set_initial_value(at(item, rooma), True)
        p.set_initial_value(weight(item), Int(1))
    p.set_initial_value(doors(rooma), {roomb, roomc})
    p.set_initial_value(doors(roomb), {rooma})
    p.set_initial_value(doors(roomc), {rooma})
    p.set_initial_value(current_load(bot1), Int(0))
    p.set_initial_value(current_load(bot2), Int(0))
    p.set_initial_value(load_limit(bot1), Int(4))
    p.set_initial_value(load_limit(bot2), Int(4))

    # Action: move(?b, ?src, ?dst)
    move = InstantaneousAction('move', b=Bot, src=Room, dst=Room)
    b, src, dst = move.parameter('b'), move.parameter('src'), move.parameter('dst')
    move.add_precondition(Equals(bot_at(b), src))
    move.add_precondition(SetMember(dst, doors(src)))
    move.add_effect(bot_at(b), dst)
    p.add_action(move)

    # Action: pick(?i, ?x, ?a, ?b)
    pick = InstantaneousAction('pick', i=Item, x=Room, a=Arm, b=Bot)
    i, x, a, b = pick.parameter('i'), pick.parameter('x'), pick.parameter('a'), pick.parameter('b')
    pick.add_precondition(at(i, x))
    pick.add_precondition(Equals(bot_at(b), x))
    pick.add_precondition(free(a))
    pick.add_precondition(mount(a, b))
    pick.add_precondition(LE(Plus(current_load(b), weight(i)), load_limit(b)))
    pick.add_effect(in_arm(i, a), True)
    pick.add_effect(at(i, x), False)
    pick.add_effect(free(a), False)
    pick.add_effect(current_load(b), Plus(current_load(b), weight(i)))
    p.add_action(pick)

    # Action: drop(?i, ?x, ?a, ?b)
    drop = InstantaneousAction('drop', i=Item, x=Room, a=Arm, b=Bot)
    i, x, a, b = drop.parameter('i'), drop.parameter('x'), drop.parameter('a'), drop.parameter('b')
    drop.add_precondition(in_arm(i, a))
    drop.add_precondition(Equals(bot_at(b), x))
    drop.add_precondition(mount(a, b))
    drop.add_effect(free(a), True)
    drop.add_effect(at(i, x), True)
    drop.add_effect(in_arm(i, a), False)
    drop.add_effect(current_load(b), Minus(current_load(b), weight(i)))
    p.add_action(drop)

    # Action: to-tray(?i, ?a, ?b)
    to_tray = InstantaneousAction('to_tray', i=Item, a=Arm, b=Bot)
    i, a, b = to_tray.parameter('i'), to_tray.parameter('a'), to_tray.parameter('b')
    to_tray.add_precondition(in_arm(i, a))
    to_tray.add_precondition(mount(a, b))
    to_tray.add_effect(free(a), True)
    to_tray.add_effect(in_arm(i, a), False)
    to_tray.add_effect(in_tray(i, b), True)
    p.add_action(to_tray)

    # Action: from-tray(?i, ?a, ?b)
    from_tray = InstantaneousAction('from_tray', i=Item, a=Arm, b=Bot)
    i, a, b = from_tray.parameter('i'), from_tray.parameter('a'), from_tray.parameter('b')
    from_tray.add_precondition(in_tray(i, b))
    from_tray.add_precondition(mount(a, b))
    from_tray.add_precondition(free(a))
    from_tray.add_effect(free(a), False)
    from_tray.add_effect(in_arm(i, a), True)
    from_tray.add_effect(in_tray(i, b), False)
    p.add_action(from_tray)

    # Goals
    p.add_goal(at(item4, roomb))
    p.add_goal(at(item3, roomb))
    p.add_goal(at(item2, roomc))
    p.add_goal(at(item1, roomc))

    return p
