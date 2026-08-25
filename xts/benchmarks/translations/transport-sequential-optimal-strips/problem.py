"""
Transport XTS (3-node / 2-truck / 2-package).
XTS features: bounded integers (capacity), sets (roads), object fluents (vat), booleans (pat, pin).
PDDL-XTS domain: xts/benchmarks/translations/transport-sequential-optimal-strips/domain.pddl
Plan length: 5 steps.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('transport_xts_3nodes')

    Location = UserType('location')
    Vehicle  = UserType('vehicle')
    Package  = UserType('package')

    city_loc_1 = Object('city-loc-1', Location)
    city_loc_2 = Object('city-loc-2', Location)
    city_loc_3 = Object('city-loc-3', Location)
    truck_1    = Object('truck-1', Vehicle)
    truck_2    = Object('truck-2', Vehicle)
    package_1  = Object('package-1', Package)
    package_2  = Object('package-2', Package)
    p.add_objects([city_loc_1, city_loc_2, city_loc_3, truck_1, truck_2, package_1, package_2])

    cap_t = IntType(0, 4)

    # Boolean predicates
    pat = Fluent('pat', p=Package, l=Location)
    pin = Fluent('pin', p=Package, v=Vehicle)
    p.add_fluent(pat, default_initial_value=False)
    p.add_fluent(pin, default_initial_value=False)

    # Set fluent: (roads ?l) - locset
    roads = Fluent('roads', SetType(Location), l=Location)
    p.add_fluent(roads, default_initial_value=set())

    # Object fluent: (vat ?v) - location
    vat = Fluent('vat', Location, v=Vehicle)
    p.add_fluent(vat)  # no default

    # Int fluent: (capacity ?v) - cap [0,4]
    capacity = Fluent('capacity', cap_t, v=Vehicle)
    p.add_fluent(capacity, default_initial_value=0)

    # Initial state
    p.set_initial_value(roads(city_loc_3), {city_loc_1, city_loc_2})
    p.set_initial_value(roads(city_loc_1), {city_loc_3})
    p.set_initial_value(roads(city_loc_2), {city_loc_3})
    p.set_initial_value(vat(truck_1), city_loc_3)
    p.set_initial_value(vat(truck_2), city_loc_1)
    p.set_initial_value(capacity(truck_1), Int(4))
    p.set_initial_value(capacity(truck_2), Int(3))
    p.set_initial_value(pat(package_1, city_loc_3), True)
    p.set_initial_value(pat(package_2, city_loc_3), True)

    # Action: drive(?v, ?l1, ?l2)
    drive = InstantaneousAction('drive', v=Vehicle, l1=Location, l2=Location)
    v, l1, l2 = drive.parameter('v'), drive.parameter('l1'), drive.parameter('l2')
    drive.add_precondition(Equals(vat(v), l1))
    drive.add_precondition(SetMember(l2, roads(l1)))
    drive.add_effect(vat(v), l2)
    p.add_action(drive)

    # Action: pick-up(?v, ?l, ?p)
    pick_up = InstantaneousAction('pick-up', v=Vehicle, l=Location, pkg=Package)
    v, l, pkg = pick_up.parameter('v'), pick_up.parameter('l'), pick_up.parameter('pkg')
    pick_up.add_precondition(Equals(vat(v), l))
    pick_up.add_precondition(pat(pkg, l))
    pick_up.add_precondition(GT(capacity(v), Int(0)))
    pick_up.add_effect(pat(pkg, l), False)
    pick_up.add_effect(pin(pkg, v), True)
    pick_up.add_effect(capacity(v), Minus(capacity(v), Int(1)))
    p.add_action(pick_up)

    # Action: drop(?v, ?l, ?p)
    drop = InstantaneousAction('drop', v=Vehicle, l=Location, pkg=Package)
    v, l, pkg = drop.parameter('v'), drop.parameter('l'), drop.parameter('pkg')
    drop.add_precondition(Equals(vat(v), l))
    drop.add_precondition(pin(pkg, v))
    drop.add_effect(pin(pkg, v), False)
    drop.add_effect(pat(pkg, l), True)
    drop.add_effect(capacity(v), Plus(capacity(v), Int(1)))
    p.add_action(drop)

    # Goal
    p.add_goal(pat(package_1, city_loc_2))
    p.add_goal(pat(package_2, city_loc_2))

    return p
