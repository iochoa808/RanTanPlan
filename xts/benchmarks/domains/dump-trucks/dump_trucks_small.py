"""
Dump Trucks (small) — sets of objects, set operations.

Features: SetType(Package), SetMember, SetAdd, SetRemove, SetUnion, SetCardinality.
Adapted from ~/unified-planning/docs/extensions/domains/dump-trucks/DumpTrucks.py
Goal: deliver 3 packages to l2 (truck t1 carries, unloads at destination).
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))

from unified_planning.shortcuts import *


def get_problem():
    p = Problem('dump_trucks_small')

    Location = UserType('Location')
    l1 = Object('l1', Location)
    l2 = Object('l2', Location)

    Truck = UserType('Truck')
    t1 = Object('t1', Truck)

    Package = UserType('Package')
    pkgs = [Object(f'p{i}', Package) for i in range(3)]

    p.add_objects([l1, l2, t1])
    p.add_objects(pkgs)

    loc_of_truck = Fluent('loc_of_truck', Location, t=Truck)
    pat = Fluent('pat', SetType(Package), l=Location)   # packages at location
    pin = Fluent('pin', SetType(Package), T=Truck)       # packages in truck
    connects = Fluent('connects', SetType(Location), l=Location)

    p.add_fluent(loc_of_truck, default_initial_value=l1)
    p.add_fluent(pat, default_initial_value=set())
    p.add_fluent(pin, default_initial_value=set())
    p.add_fluent(connects, default_initial_value=set())

    p.set_initial_value(loc_of_truck(t1), l1)
    p.set_initial_value(pat(l1), set(pkgs))
    p.set_initial_value(pat(l2), set())      # explicit empty-set init (UP default not stored)
    p.set_initial_value(pin(t1), set())      # explicit empty-set init
    p.set_initial_value(connects(l1), {l2})
    p.set_initial_value(connects(l2), {l1})

    move_truck = InstantaneousAction('move_truck', t=Truck, lfrom=Location, lto=Location)
    t, lfrom, lto = [move_truck.parameter(x) for x in ('t', 'lfrom', 'lto')]
    move_truck.add_precondition(SetMember(lto, connects(lfrom)))
    move_truck.add_precondition(Equals(loc_of_truck(t), lfrom))
    move_truck.add_effect(loc_of_truck(t), lto)
    p.add_action(move_truck)

    load_truck = InstantaneousAction('load_truck', pkg=Package, t=Truck, l=Location)
    pkg, t, l = [load_truck.parameter(x) for x in ('pkg', 't', 'l')]
    load_truck.add_precondition(Equals(l, loc_of_truck(t)))
    load_truck.add_precondition(SetMember(pkg, pat(l)))
    load_truck.add_effect(pat(l), SetRemove(pkg, pat(l)))
    load_truck.add_effect(pin(t), SetAdd(pkg, pin(t)))
    p.add_action(load_truck)

    unload_truck = InstantaneousAction('unload_truck', t=Truck, l=Location)
    t, l = [unload_truck.parameter(x) for x in ('t', 'l')]
    unload_truck.add_precondition(Equals(l, loc_of_truck(t)))
    unload_truck.add_effect(pat(l), SetUnion(pat(l), pin(t)))
    unload_truck.add_effect(pin(t), set())
    p.add_action(unload_truck)

    # Goal: all 3 packages at l2
    p.add_goal(Equals(pat(l2), set(pkgs)))

    return p
