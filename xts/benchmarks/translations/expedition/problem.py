"""
expedition: 2 sleds, each needs to traverse 2 waypoints.
s0: wa0->wa1->wa2 (retrieve supply from wa0 first).
s1: wb0->wb1->wb2 (retrieve supply from wb0 first).
Plan: 6 steps (interleaved).
PDDL-XTS: expedition
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem("expedition-2-sled-1-xts")

    Sled     = UserType("sled")
    Waypoint = UserType("waypoint")
    supply_t = IntType(0, 9)

    is_next        = Fluent("is_next",          x=Waypoint, y=Waypoint)
    sled_at        = Fluent("sled-at",          Waypoint, s=Sled)
    sled_supplies  = Fluent("sled_supplies",    supply_t, s=Sled)
    sled_capacity  = Fluent("sled_capacity",    supply_t, s=Sled)
    wp_supplies    = Fluent("waypoint_supplies", supply_t, w=Waypoint)

    p.add_fluent(is_next, default_initial_value=False)
    p.add_fluent(sled_at)          # object fluent — no default
    p.add_fluent(sled_supplies,    default_initial_value=0)
    p.add_fluent(sled_capacity,    default_initial_value=0)
    p.add_fluent(wp_supplies,      default_initial_value=0)

    s0  = Object("s0",  Sled)
    s1  = Object("s1",  Sled)
    wa0 = Object("wa0", Waypoint)
    wa1 = Object("wa1", Waypoint)
    wa2 = Object("wa2", Waypoint)
    wb0 = Object("wb0", Waypoint)
    wb1 = Object("wb1", Waypoint)
    wb2 = Object("wb2", Waypoint)
    p.add_objects([s0, s1, wa0, wa1, wa2, wb0, wb1, wb2])

    p.set_initial_value(sled_at(s0),         wa0)
    p.set_initial_value(sled_capacity(s0),   Int(4))
    p.set_initial_value(sled_supplies(s0),   Int(1))
    p.set_initial_value(wp_supplies(wa0),    Int(5))
    p.set_initial_value(wp_supplies(wa1),    Int(0))
    p.set_initial_value(wp_supplies(wa2),    Int(0))
    p.set_initial_value(is_next(wa0, wa1),   True)
    p.set_initial_value(is_next(wa1, wa2),   True)

    p.set_initial_value(sled_at(s1),         wb0)
    p.set_initial_value(sled_capacity(s1),   Int(4))
    p.set_initial_value(sled_supplies(s1),   Int(1))
    p.set_initial_value(wp_supplies(wb0),    Int(5))
    p.set_initial_value(wp_supplies(wb1),    Int(0))
    p.set_initial_value(wp_supplies(wb2),    Int(0))
    p.set_initial_value(is_next(wb0, wb1),   True)
    p.set_initial_value(is_next(wb1, wb2),   True)

    # move_forwards(?s, ?w1, ?w2)
    move_fwd = InstantaneousAction("move_forwards", s=Sled, w1=Waypoint, w2=Waypoint)
    s, w1, w2 = (move_fwd.parameter(n) for n in ("s","w1","w2"))
    move_fwd.add_precondition(Equals(sled_at(s), w1))
    move_fwd.add_precondition(GE(sled_supplies(s), Int(1)))
    move_fwd.add_precondition(is_next(w1, w2))
    move_fwd.add_effect(sled_at(s),        w2)
    move_fwd.add_effect(sled_supplies(s),  Minus(sled_supplies(s), Int(1)))
    p.add_action(move_fwd)

    # move_backwards(?s, ?w1, ?w2)
    move_bwd = InstantaneousAction("move_backwards", s=Sled, w1=Waypoint, w2=Waypoint)
    s, w1, w2 = (move_bwd.parameter(n) for n in ("s","w1","w2"))
    move_bwd.add_precondition(Equals(sled_at(s), w1))
    move_bwd.add_precondition(GE(sled_supplies(s), Int(1)))
    move_bwd.add_precondition(is_next(w2, w1))
    move_bwd.add_effect(sled_at(s),       w2)
    move_bwd.add_effect(sled_supplies(s), Minus(sled_supplies(s), Int(1)))
    p.add_action(move_bwd)

    # store_supplies(?s, ?w)
    store = InstantaneousAction("store_supplies", s=Sled, w=Waypoint)
    s, w = (store.parameter(n) for n in ("s","w"))
    store.add_precondition(Equals(sled_at(s), w))
    store.add_precondition(GE(sled_supplies(s), Int(1)))
    store.add_effect(wp_supplies(w),   Plus(wp_supplies(w), Int(1)))
    store.add_effect(sled_supplies(s), Minus(sled_supplies(s), Int(1)))
    p.add_action(store)

    # retrieve_supplies(?s, ?w)
    retrieve = InstantaneousAction("retrieve_supplies", s=Sled, w=Waypoint)
    s, w = (retrieve.parameter(n) for n in ("s","w"))
    retrieve.add_precondition(Equals(sled_at(s), w))
    retrieve.add_precondition(GE(wp_supplies(w), Int(1)))
    retrieve.add_precondition(GT(sled_capacity(s), sled_supplies(s)))
    retrieve.add_effect(wp_supplies(w),   Minus(wp_supplies(w), Int(1)))
    retrieve.add_effect(sled_supplies(s), Plus(sled_supplies(s), Int(1)))
    p.add_action(retrieve)

    p.add_goal(Equals(sled_at(s0), wa2))
    p.add_goal(Equals(sled_at(s1), wb2))
    return p
