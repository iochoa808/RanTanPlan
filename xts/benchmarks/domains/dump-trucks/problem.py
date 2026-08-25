"""
Dump Trucks — full i0 instance: 2 trucks, 10 packages, 2 locations.

Features: SetType, SetMember, SetAdd, SetRemove, SetUnion, SetCardinality.
Goal: union of loaded packages > 5, and t1 carries strictly fewer than t2.
PDDL-XTS counterpart: PDDL-XTS/dump-trucks/instances/i0.pddl
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('dump_trucks_i0')

    Location = UserType('Location')
    l1 = Object('l1', Location)
    l2 = Object('l2', Location)

    Truck = UserType('Truck')
    t1 = Object('t1', Truck)
    t2 = Object('t2', Truck)

    Package = UserType('Package')
    pkgs = [Object(f'p{i}', Package) for i in range(10)]

    p.add_objects([l1, l2, t1, t2])
    p.add_objects(pkgs)

    truck_at  = Fluent('truck_at',   Location,         t=Truck)
    package_at = Fluent('package_at', SetType(Package), l=Location)
    package_in = Fluent('package_in', SetType(Package), t=Truck)
    connects  = Fluent('connects',   SetType(Location), l=Location)

    p.add_fluent(truck_at,   default_initial_value=l1)
    p.add_fluent(package_at, default_initial_value=set())
    p.add_fluent(package_in, default_initial_value=set())
    p.add_fluent(connects,   default_initial_value=set())

    p.set_initial_value(truck_at(t1), l1)
    p.set_initial_value(truck_at(t2), l2)
    p.set_initial_value(package_at(l1), set(pkgs))
    p.set_initial_value(package_at(l2), set())
    p.set_initial_value(package_in(t1), set())
    p.set_initial_value(package_in(t2), set())
    p.set_initial_value(connects(l1), {l2})
    p.set_initial_value(connects(l2), {l1})

    move_truck = InstantaneousAction('move_truck', t=Truck, frm=Location, to=Location)
    t, frm, to = [move_truck.parameter(x) for x in ('t', 'frm', 'to')]
    move_truck.add_precondition(SetMember(to, connects(frm)))
    move_truck.add_precondition(Equals(truck_at(t), frm))
    move_truck.add_effect(truck_at(t), to)
    p.add_action(move_truck)

    load_truck = InstantaneousAction('load_truck', pkg=Package, t=Truck, l=Location)
    pkg, t, l = [load_truck.parameter(x) for x in ('pkg', 't', 'l')]
    load_truck.add_precondition(Equals(truck_at(t), l))
    load_truck.add_precondition(SetMember(pkg, package_at(l)))
    load_truck.add_precondition(LT(SetCardinality(package_in(t)), 5))
    load_truck.add_effect(package_at(l), SetRemove(pkg, package_at(l)))
    load_truck.add_effect(package_in(t), SetAdd(pkg, package_in(t)))
    p.add_action(load_truck)

    unload_truck = InstantaneousAction('unload_truck', t=Truck, l=Location)
    t, l = [unload_truck.parameter(x) for x in ('t', 'l')]
    unload_truck.add_precondition(Equals(truck_at(t), l))
    unload_truck.add_effect(package_at(l), SetUnion(package_at(l), package_in(t)))
    unload_truck.add_effect(package_in(t), set())
    p.add_action(unload_truck)

    union_loaded = SetUnion(package_in(t1), package_in(t2))
    p.add_goal(GT(SetCardinality(union_loaded), 5))
    p.add_goal(LT(SetCardinality(package_in(t1)), SetCardinality(package_in(t2))))

    return p
