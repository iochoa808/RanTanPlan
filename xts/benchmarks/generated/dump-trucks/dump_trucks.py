"""
Shared generator for the Dump Trucks benchmark (paper: "5 instances with
10-20 packages, 2 locations, 2 trucks... we tried to mimic the problems
from (Gregory et al. 2012). As the software is not publicly available,
direct performance comparison cannot be conducted."). No instances.txt or
fixed instance files ship in the reference repo for this domain -- only
DumpTrucks.py's single hardcoded n_packages=10 example -- so the 5
instances here are parametrically generated at n_packages in
{10, 12, 15, 18, 20}, evenly spanning the paper's stated 10-20 range.

Replicates the exact UP model in
~/unified-planning/docs/extensions/domains/dump-trucks/DumpTrucks.py
(generalized from its hardcoded n_packages=10): 2 locations, 2 trucks,
n_packages packages, SetType fluents (no arrays) for packages-at-location,
packages-in-truck, and location connectivity. 3 actions (move/load/unload);
goal: >5 packages loaded across both trucks combined, with truck 1 holding
strictly fewer than truck 2.

The paper explicitly notes count-expression compilation blows up for this
domain at ~20 objects (600+ seconds) -- expect the n_packages=20 instance
to be very slow or infeasible to compile through the 'up'/iasciu pipeline;
that matches the paper's own finding, not a bug here.
"""
import math
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))

from unified_planning.shortcuts import *


def build_dump_trucks(name: str, n_packages: int):
    p = Problem(name)

    Location = UserType('Location')
    l1 = Object('l1', Location)
    l2 = Object('l2', Location)

    Truck = UserType('Truck')
    t1 = Object('t1', Truck)
    t2 = Object('t2', Truck)

    Package = UserType('Package')
    packages = [Object(f'p{i + 1}', Package) for i in range(n_packages)]

    p.add_objects([l1, l2, t1, t2])
    p.add_objects(packages)

    loc_of_truck = Fluent('loc_of_truck', Location, t=Truck)
    pat = Fluent('pat', SetType(Package), l=Location)
    pin = Fluent('pin', SetType(Package), T=Truck)
    connects = Fluent('connects', SetType(Location), l=Location)

    p.add_fluent(loc_of_truck, default_initial_value=l1)
    p.add_fluent(pat, default_initial_value=set())
    p.add_fluent(pin, default_initial_value=set())
    p.add_fluent(connects, default_initial_value=set())

    p.set_initial_value(loc_of_truck(t1), l1)
    p.set_initial_value(loc_of_truck(t2), l2)
    p.set_initial_value(pat(l1), {*packages})
    # RTP's native ingestion requires an explicit :init entry for every
    # object/set fluent -- default_initial_value alone isn't enough, same
    # issue as elsewhere in this batch (labyrinth_ipc.py's robot_at etc.),
    # here for the fluents the original script left at their defaults.
    p.set_initial_value(pat(l2), set())
    p.set_initial_value(pin(t1), set())
    p.set_initial_value(pin(t2), set())
    p.set_initial_value(connects(l1), {l2})
    p.set_initial_value(connects(l2), {l1})

    move_truck = InstantaneousAction('move_truck', t=Truck, lfrom=Location, lto=Location)
    t, lfrom, lto = move_truck.parameter('t'), move_truck.parameter('lfrom'), move_truck.parameter('lto')
    move_truck.add_precondition(SetMember(lto, connects(lfrom)))
    move_truck.add_precondition(Equals(loc_of_truck(t), lfrom))
    move_truck.add_effect(loc_of_truck(t), lto)

    load_truck = InstantaneousAction('load_truck', p_=Package, t=Truck, l=Location)
    p_, t, l = load_truck.parameter('p_'), load_truck.parameter('t'), load_truck.parameter('l')
    load_truck.add_precondition(Equals(l, loc_of_truck(t)))
    load_truck.add_precondition(SetMember(p_, pat(l)))
    load_truck.add_precondition(LT(SetCardinality(pin(t)), math.ceil(n_packages / 2)))
    load_truck.add_effect(pat(l), SetRemove(p_, pat(l)))
    load_truck.add_effect(pin(t), SetAdd(p_, pin(t)))

    unload_truck = InstantaneousAction('unload_truck', t=Truck, l=Location)
    t, l = unload_truck.parameter('t'), unload_truck.parameter('l')
    unload_truck.add_precondition(Equals(l, loc_of_truck(t)))
    unload_truck.add_effect(pat(l), SetUnion(pat(l), pin(t)))
    unload_truck.add_effect(pin(t), set())

    p.add_actions([move_truck, load_truck, unload_truck])

    p.add_goal(And(
        GT(SetCardinality(SetUnion(pin(t1), pin(t2))), 5),
        LT(SetCardinality(pin(t1)), SetCardinality(pin(t2)))
    ))

    return p


SIZES = {
    "dt_n10": 10,
    "dt_n12": 12,
    "dt_n15": 15,
    "dt_n18": 18,
    "dt_n20": 20,
}
