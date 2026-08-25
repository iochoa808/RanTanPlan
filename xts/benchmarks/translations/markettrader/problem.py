"""
Market Trader XTS — buy apples at Market1, sell at Market2, buy bananas at Market2, sell at Market1.
XTS features: bounded integers (money, capacity, stock), sets (roads), object fluents (camel-at).
Plan: 8 steps.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('minimal_trader_xts')

    Place    = UserType('place')
    Locatable = UserType('locatable')
    Market   = UserType('market', father=Place)
    Camel    = UserType('camel',  father=Locatable)
    Goods    = UserType('goods',  father=Locatable)

    money_t = IntType(0, 63)
    cap_t   = IntType(0, 25)
    stock_t = IntType(0, 15)

    # Set fluent: (roads ?m) - marketset
    roads = Fluent('roads', SetType(Market), m=Market)
    p.add_fluent(roads, default_initial_value=set())

    # Object fluent: (camel-at ?t) - market
    camel_at = Fluent('camel_at', Market, t=Camel)
    p.add_fluent(camel_at)

    # Bounded int fluents
    on_sale    = Fluent('on_sale', stock_t, g=Goods, m=Market)
    drive_cost = Fluent('drive_cost', money_t, p1=Market, p2=Market)
    price      = Fluent('price', money_t, g=Goods, m=Market)
    bought     = Fluent('bought', stock_t, g=Goods)
    cash       = Fluent('cash', money_t)
    capacity   = Fluent('capacity', cap_t)
    p.add_fluent(on_sale,    default_initial_value=0)
    p.add_fluent(drive_cost, default_initial_value=0)
    p.add_fluent(price,      default_initial_value=0)
    p.add_fluent(bought,     default_initial_value=0)
    p.add_fluent(cash,       default_initial_value=0)
    p.add_fluent(capacity,   default_initial_value=0)

    # Objects
    Market1 = Object('Market1', Market)
    Market2 = Object('Market2', Market)
    camel1  = Object('camel1',  Camel)
    Apple   = Object('Apple',   Goods)
    Banana  = Object('Banana',  Goods)
    p.add_objects([Market1, Market2, camel1, Apple, Banana])

    # Initial state
    p.set_initial_value(price(Apple, Market1),  Int(5))
    p.set_initial_value(on_sale(Apple, Market1), Int(10))
    p.set_initial_value(price(Banana, Market1), Int(20))
    p.set_initial_value(on_sale(Banana, Market1), Int(0))
    p.set_initial_value(price(Apple, Market2),  Int(15))
    p.set_initial_value(on_sale(Apple, Market2), Int(0))
    p.set_initial_value(price(Banana, Market2), Int(10))
    p.set_initial_value(on_sale(Banana, Market2), Int(5))
    p.set_initial_value(bought(Apple),  Int(0))
    p.set_initial_value(bought(Banana), Int(0))
    p.set_initial_value(drive_cost(Market1, Market1), Int(0))
    p.set_initial_value(drive_cost(Market1, Market2), Int(2))
    p.set_initial_value(drive_cost(Market2, Market1), Int(2))
    p.set_initial_value(drive_cost(Market2, Market2), Int(0))
    p.set_initial_value(roads(Market1), {Market2})
    p.set_initial_value(roads(Market2), {Market1})
    p.set_initial_value(camel_at(camel1), Market1)
    p.set_initial_value(cash, Int(20))
    p.set_initial_value(capacity, Int(2))

    # Action: travel(?t, ?from, ?to)
    travel = InstantaneousAction('travel', t=Camel, frm=Market, to=Market)
    t, frm, to = travel.parameter('t'), travel.parameter('frm'), travel.parameter('to')
    travel.add_precondition(SetMember(to, roads(frm)))
    travel.add_precondition(GE(cash, drive_cost(frm, to)))
    travel.add_precondition(Equals(camel_at(t), frm))
    travel.add_effect(cash, Minus(cash, drive_cost(frm, to)))
    travel.add_effect(camel_at(t), to)
    p.add_action(travel)

    # Action: buy(?t, ?g, ?m)
    buy = InstantaneousAction('buy', t=Camel, g=Goods, m=Market)
    t, g, m = buy.parameter('t'), buy.parameter('g'), buy.parameter('m')
    buy.add_precondition(Equals(camel_at(t), m))
    buy.add_precondition(LE(Plus(Int(7), price(g, m)), cash))
    buy.add_precondition(GE(capacity, Int(1)))
    buy.add_precondition(GT(on_sale(g, m), Int(0)))
    buy.add_effect(capacity, Minus(capacity, Int(1)))
    buy.add_effect(bought(g), Plus(bought(g), Int(1)))
    buy.add_effect(cash, Minus(cash, price(g, m)))
    buy.add_effect(on_sale(g, m), Minus(on_sale(g, m), Int(1)))
    p.add_action(buy)

    # Action: sell(?t, ?g, ?m)
    sell = InstantaneousAction('sell', t=Camel, g=Goods, m=Market)
    t, g, m = sell.parameter('t'), sell.parameter('g'), sell.parameter('m')
    sell.add_precondition(Equals(camel_at(t), m))
    sell.add_precondition(GE(bought(g), Int(1)))
    sell.add_precondition(LT(cash, Int(63)))
    sell.add_effect(capacity, Plus(capacity, Int(1)))
    sell.add_effect(bought(g), Minus(bought(g), Int(1)))
    sell.add_effect(cash, Plus(cash, price(g, m)))
    p.add_action(sell)

    # Goal
    p.add_goal(GE(cash, Int(40)))

    return p
