"""
tpp-propositional XTS. Features: bounded integers + sets + object fluent.
Expected: 5 steps (drive->buy->load->drive->unload).
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem("TPP_prop_xts")

    Place    = UserType("place")
    Locatable= UserType("locatable")
    Depot    = UserType("depot",  father=Place)
    Market   = UserType("market", father=Place)
    Truck    = UserType("truck",  father=Locatable)
    Goods    = UserType("goods",  father=Locatable)
    lvl_t    = IntType(0, 1)

    goods1  = Object("goods1",  Goods)
    truck1  = Object("truck1",  Truck)
    market1 = Object("market1", Market)
    depot1  = Object("depot1",  Depot)
    p.add_objects([goods1, truck1, market1, depot1])

    # Object fluent: (truck-at ?t) - place
    truck_at = Fluent("truck-at", Place, t=Truck)
    p.add_fluent(truck_at)

    # Set fluent: (connections ?p) - set(place)
    connections = Fluent("connections", SetType(Place), p=Place)
    p.add_fluent(connections, default_initial_value=set())

    # Int fluents (bounded [0,1])
    loaded        = Fluent("loaded",         lvl_t, g=Goods, t=Truck)
    ready_to_load = Fluent("ready-to-load",  lvl_t, g=Goods, m=Market)
    stored        = Fluent("stored",          lvl_t, g=Goods)
    on_sale       = Fluent("on-sale",         lvl_t, g=Goods, m=Market)
    for f in [loaded, ready_to_load, stored, on_sale]:
        p.add_fluent(f, default_initial_value=0)

    # Initial state
    p.set_initial_value(truck_at(truck1), depot1)
    p.set_initial_value(connections(depot1),  {market1})
    p.set_initial_value(connections(market1), {depot1})
    p.set_initial_value(ready_to_load(goods1, market1), Int(0))
    p.set_initial_value(stored(goods1),                 Int(0))
    p.set_initial_value(loaded(goods1, truck1),         Int(0))
    p.set_initial_value(on_sale(goods1, market1),       Int(1))

    # drive(?t, ?from, ?to)
    drive = InstantaneousAction("drive", t=Truck, frm=Place, to=Place)
    t_p, frm_p, to_p = drive.parameter("t"), drive.parameter("frm"), drive.parameter("to")
    drive.add_precondition(Equals(truck_at(t_p), frm_p))
    drive.add_precondition(SetMember(to_p, connections(frm_p)))
    drive.add_effect(truck_at(t_p), to_p)
    p.add_action(drive)

    # buy(?t, ?g, ?m)
    buy = InstantaneousAction("buy", t=Truck, g=Goods, m=Market)
    t_p, g_p, m_p = buy.parameter("t"), buy.parameter("g"), buy.parameter("m")
    buy.add_precondition(Equals(truck_at(t_p), m_p))
    buy.add_precondition(GE(on_sale(g_p, m_p), Int(1)))
    buy.add_precondition(LT(ready_to_load(g_p, m_p), Int(1)))
    buy.add_effect(on_sale(g_p, m_p),       Minus(on_sale(g_p, m_p), Int(1)))
    buy.add_effect(ready_to_load(g_p, m_p), Plus(ready_to_load(g_p, m_p), Int(1)))
    p.add_action(buy)

    # load(?g, ?t, ?m)
    load = InstantaneousAction("load", g=Goods, t=Truck, m=Market)
    g_p, t_p, m_p = load.parameter("g"), load.parameter("t"), load.parameter("m")
    load.add_precondition(Equals(truck_at(t_p), m_p))
    load.add_precondition(GE(ready_to_load(g_p, m_p), Int(1)))
    load.add_precondition(LT(loaded(g_p, t_p), Int(1)))
    load.add_effect(ready_to_load(g_p, m_p), Minus(ready_to_load(g_p, m_p), Int(1)))
    load.add_effect(loaded(g_p, t_p),         Plus(loaded(g_p, t_p), Int(1)))
    p.add_action(load)

    # unload(?g, ?t, ?d)
    unload = InstantaneousAction("unload", g=Goods, t=Truck, d=Depot)
    g_p, t_p, d_p = unload.parameter("g"), unload.parameter("t"), unload.parameter("d")
    unload.add_precondition(Equals(truck_at(t_p), d_p))
    unload.add_precondition(GE(loaded(g_p, t_p), Int(1)))
    unload.add_precondition(LT(stored(g_p), Int(1)))
    unload.add_effect(loaded(g_p, t_p), Minus(loaded(g_p, t_p), Int(1)))
    unload.add_effect(stored(g_p),       Plus(stored(g_p), Int(1)))
    p.add_action(unload)

    # Goal
    p.add_goal(Equals(stored(goods1), Int(1)))

    return p
