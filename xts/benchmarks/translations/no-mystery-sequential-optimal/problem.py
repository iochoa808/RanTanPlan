"""
No-Mystery XTS (minimal transport: 2 locations, 1 package, 1 truck).
XTS features: bounded integers (fuel), sets (connections), object fluents (tloc), booleans (pat, pin).
PDDL-XTS domain: xts/benchmarks/translations/no-mystery-sequential-optimal/domain.pddl
Plan length: 3 steps.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('no_mystery_xts_minimal')

    Location = UserType('location')
    Truck    = UserType('truck')
    Package  = UserType('package')

    loc1     = Object('loc1', Location)
    loc2     = Object('loc2', Location)
    package1 = Object('package1', Package)
    truck1   = Object('truck1', Truck)
    p.add_objects([loc1, loc2, package1, truck1])

    flevel_t = IntType(0, 2)

    # Boolean predicates
    pat = Fluent('pat', p=Package, l=Location)
    pin = Fluent('pin', p=Package, t=Truck)
    p.add_fluent(pat, default_initial_value=False)
    p.add_fluent(pin, default_initial_value=False)

    # Set fluent: (connections ?l) - locset
    connections = Fluent('connections', SetType(Location), l=Location)
    p.add_fluent(connections, default_initial_value=set())

    # Int fluent: (fuel ?t) - flevel [0,2]
    fuel = Fluent('fuel', flevel_t, t=Truck)
    p.add_fluent(fuel, default_initial_value=0)

    # Object fluent: (tloc ?t) - location
    tloc = Fluent('tloc', Location, t=Truck)
    p.add_fluent(tloc)  # no default

    # Initial state
    p.set_initial_value(connections(loc1), {loc2})
    p.set_initial_value(connections(loc2), {loc1})
    p.set_initial_value(tloc(truck1), loc1)
    p.set_initial_value(fuel(truck1), Int(2))
    p.set_initial_value(pat(package1, loc1), True)

    # Action: load(?p, ?t, ?l)
    load = InstantaneousAction('load', pkg=Package, t=Truck, l=Location)
    pkg, t, l = load.parameter('pkg'), load.parameter('t'), load.parameter('l')
    load.add_precondition(Equals(tloc(t), l))
    load.add_precondition(pat(pkg, l))
    load.add_effect(pat(pkg, l), False)
    load.add_effect(pin(pkg, t), True)
    p.add_action(load)

    # Action: unload(?p, ?t, ?l)
    unload = InstantaneousAction('unload', pkg=Package, t=Truck, l=Location)
    pkg, t, l = unload.parameter('pkg'), unload.parameter('t'), unload.parameter('l')
    unload.add_precondition(Equals(tloc(t), l))
    unload.add_precondition(pin(pkg, t))
    unload.add_effect(pat(pkg, l), True)
    unload.add_effect(pin(pkg, t), False)
    p.add_action(unload)

    # Action: drive(?t, ?l1, ?l2)
    drive = InstantaneousAction('drive', t=Truck, l1=Location, l2=Location)
    t, l1, l2 = drive.parameter('t'), drive.parameter('l1'), drive.parameter('l2')
    drive.add_precondition(SetMember(l2, connections(l1)))
    drive.add_precondition(Equals(tloc(t), l1))
    drive.add_precondition(GE(fuel(t), Int(1)))
    drive.add_effect(tloc(t), l2)
    drive.add_effect(fuel(t), Minus(fuel(t), Int(1)))
    p.add_action(drive)

    # Goal
    p.add_goal(pat(package1, loc2))

    return p
