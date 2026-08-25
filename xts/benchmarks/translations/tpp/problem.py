"""
tpp XTS. Features: bounded integers + object fluent (loc-of).
Expected: 3 steps.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem("simple_tpp_xts")

    Place    = UserType("place")
    Locatable= UserType("locatable")
    Depot    = UserType("depot",   father=Place)
    Market   = UserType("market",  father=Place)
    Truck    = UserType("truck",   father=Locatable)
    Goods    = UserType("goods",   father=Locatable)
    qty_t    = IntType(0, 15)

    market1 = Object("market1", Market)
    market2 = Object("market2", Market)
    depot0  = Object("depot0",  Depot)
    truck0  = Object("truck0",  Truck)
    goods0  = Object("goods0",  Goods)
    p.add_objects([market1, market2, depot0, truck0, goods0])

    # Object fluent: (loc-of ?t) - place
    loc_of  = Fluent("loc-of",  Place,  t=Truck)
    p.add_fluent(loc_of)

    # Int fluents
    on_sale = Fluent("on-sale", qty_t, g=Goods, m=Market)
    bought  = Fluent("bought",  qty_t, g=Goods)
    request = Fluent("request", qty_t, g=Goods)
    p.add_fluent(on_sale, default_initial_value=0)
    p.add_fluent(bought,  default_initial_value=0)
    p.add_fluent(request, default_initial_value=0)

    # Initial state
    p.set_initial_value(on_sale(goods0, market1), Int(5))
    p.set_initial_value(on_sale(goods0, market2), Int(10))
    p.set_initial_value(loc_of(truck0),            depot0)
    p.set_initial_value(bought(goods0),            Int(0))
    p.set_initial_value(request(goods0),           Int(7))

    # drive(?t, ?from, ?to)
    drive = InstantaneousAction("drive", t=Truck, frm=Place, to=Place)
    t_p, frm_p, to_p = drive.parameter("t"), drive.parameter("frm"), drive.parameter("to")
    drive.add_precondition(Equals(loc_of(t_p), frm_p))
    drive.add_effect(loc_of(t_p), to_p)
    p.add_action(drive)

    # buy-allneeded(?t, ?g, ?m): on_sale > request-bought
    ban = InstantaneousAction("buy-allneeded", t=Truck, g=Goods, m=Market)
    t_p, g_p, m_p = ban.parameter("t"), ban.parameter("g"), ban.parameter("m")
    ban.add_precondition(Equals(loc_of(t_p), m_p))
    ban.add_precondition(GT(on_sale(g_p, m_p), Int(0)))
    ban.add_precondition(GT(on_sale(g_p, m_p), Minus(request(g_p), bought(g_p))))
    ban.add_effect(on_sale(g_p, m_p), Minus(on_sale(g_p, m_p), Minus(request(g_p), bought(g_p))))
    ban.add_effect(bought(g_p), request(g_p))
    p.add_action(ban)

    # buy-all(?t, ?g, ?m): on_sale <= request-bought
    ba = InstantaneousAction("buy-all", t=Truck, g=Goods, m=Market)
    t_p, g_p, m_p = ba.parameter("t"), ba.parameter("g"), ba.parameter("m")
    ba.add_precondition(Equals(loc_of(t_p), m_p))
    ba.add_precondition(GT(on_sale(g_p, m_p), Int(0)))
    ba.add_precondition(LE(on_sale(g_p, m_p), Minus(request(g_p), bought(g_p))))
    ba.add_effect(bought(g_p), Plus(bought(g_p), on_sale(g_p, m_p)))
    ba.add_effect(on_sale(g_p, m_p), Int(0))
    p.add_action(ba)

    # Goal
    p.add_goal(GE(bought(goods0), request(goods0)))
    p.add_goal(Equals(loc_of(truck0), depot0))

    return p
