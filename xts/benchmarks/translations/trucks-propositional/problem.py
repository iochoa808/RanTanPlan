"""
Trucks-Propositional XTS — truck-1.
Features: bounded ints (current-time, deadline), object fluent (truck-at),
          forall/imply in preconditions (QR expands object-typed forall).
Plan: ~13 steps.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *
from unified_planning.model import Variable


def get_problem():
    p = Problem('truck-1-xts')

    Truckarea = UserType('truckarea')
    Location  = UserType('location')
    Locatable = UserType('locatable')
    Truck     = UserType('truck',   father=Locatable)
    Package   = UserType('package', father=Locatable)

    time_t = IntType(0, 6)

    # Objects
    truck1   = Object('truck1',   Truck)
    package1 = Object('package1', Package)
    package2 = Object('package2', Package)
    package3 = Object('package3', Package)
    l1 = Object('l1', Location)
    l2 = Object('l2', Location)
    l3 = Object('l3', Location)
    a1 = Object('a1', Truckarea)
    a2 = Object('a2', Truckarea)

    p.add_objects([truck1, package1, package2, package3,
                   l1, l2, l3, a1, a2])

    # Fluents
    at_pkg       = Fluent('at',             p=Package,   l=Location)
    in_pkg       = Fluent('in',             p=Package,   t=Truck, a=Truckarea)
    free_area    = Fluent('free',           a=Truckarea, t=Truck)
    at_dest      = Fluent('at-destination', p=Package,   l=Location)
    closer       = Fluent('closer',         a1=Truckarea, a2=Truckarea)
    connected    = Fluent('connected',      l1=Location, l2=Location)
    truck_at     = Fluent('truck-at',       Location,    t=Truck)
    current_time = Fluent('current-time',   time_t)
    deadline     = Fluent('deadline',       time_t,      p=Package)

    p.add_fluent(at_pkg,       default_initial_value=False)
    p.add_fluent(in_pkg,       default_initial_value=False)
    p.add_fluent(free_area,    default_initial_value=False)
    p.add_fluent(at_dest,      default_initial_value=False)
    p.add_fluent(closer,       default_initial_value=False)
    p.add_fluent(connected,    default_initial_value=False)
    p.add_fluent(truck_at,     default_initial_value=l1)
    p.add_fluent(current_time, default_initial_value=Int(0))
    p.add_fluent(deadline,     default_initial_value=Int(0))

    # Init
    p.set_initial_value(truck_at(truck1), l3)
    p.set_initial_value(free_area(a1, truck1), True)
    p.set_initial_value(free_area(a2, truck1), True)
    p.set_initial_value(at_pkg(package1, l2), True)
    p.set_initial_value(at_pkg(package2, l2), True)
    p.set_initial_value(at_pkg(package3, l2), True)

    p.set_initial_value(connected(l1, l2), True)
    p.set_initial_value(connected(l1, l3), True)
    p.set_initial_value(connected(l2, l1), True)
    p.set_initial_value(connected(l2, l3), True)
    p.set_initial_value(connected(l3, l1), True)
    p.set_initial_value(connected(l3, l2), True)

    p.set_initial_value(closer(a1, a2), True)

    p.set_initial_value(current_time, Int(0))
    p.set_initial_value(deadline(package1), Int(3))
    p.set_initial_value(deadline(package2), Int(6))
    p.set_initial_value(deadline(package3), Int(6))

    # ---- Actions ----

    # load(?p, ?t, ?a1, ?l)  with forall(?a2) imply(closer(?a2,?a1), free(?a2,?t))
    load = InstantaneousAction('load', pkg=Package, t=Truck, a1=Truckarea, l=Location)
    pkg_p, t_p, a1_p, l_p = [load.parameter(x) for x in ('pkg', 't', 'a1', 'l')]
    a2v = Variable('a2', Truckarea)
    load.add_precondition(Equals(truck_at(t_p), l_p))
    load.add_precondition(at_pkg(pkg_p, l_p))
    load.add_precondition(free_area(a1_p, t_p))
    load.add_precondition(Forall(Implies(closer(a2v, a1_p), free_area(a2v, t_p)), a2v))
    load.add_effect(at_pkg(pkg_p, l_p),  False)
    load.add_effect(free_area(a1_p, t_p), False)
    load.add_effect(in_pkg(pkg_p, t_p, a1_p), True)
    p.add_action(load)

    # unload(?p, ?t, ?a1, ?l)  with forall(?a2) imply(closer(?a2,?a1), free(?a2,?t))
    unload = InstantaneousAction('unload', pkg=Package, t=Truck, a1=Truckarea, l=Location)
    pkg_p, t_p, a1_p, l_p = [unload.parameter(x) for x in ('pkg', 't', 'a1', 'l')]
    a2v = Variable('a2', Truckarea)
    unload.add_precondition(Equals(truck_at(t_p), l_p))
    unload.add_precondition(in_pkg(pkg_p, t_p, a1_p))
    unload.add_precondition(Forall(Implies(closer(a2v, a1_p), free_area(a2v, t_p)), a2v))
    unload.add_effect(in_pkg(pkg_p, t_p, a1_p), False)
    unload.add_effect(free_area(a1_p, t_p),      True)
    unload.add_effect(at_pkg(pkg_p, l_p),         True)
    p.add_action(unload)

    # drive(?t, ?from, ?to)
    drive = InstantaneousAction('drive', t=Truck, src=Location, dst=Location)
    t_p, src_p, dst_p = [drive.parameter(x) for x in ('t', 'src', 'dst')]
    drive.add_precondition(Equals(truck_at(t_p), src_p))
    drive.add_precondition(connected(src_p, dst_p))
    drive.add_effect(truck_at(t_p),    dst_p)
    drive.add_effect(current_time,     Plus(current_time, Int(1)))
    p.add_action(drive)

    # deliver(?p, ?l)
    deliver = InstantaneousAction('deliver', pkg=Package, l=Location)
    pkg_p, l_p = deliver.parameter('pkg'), deliver.parameter('l')
    deliver.add_precondition(at_pkg(pkg_p, l_p))
    deliver.add_precondition(LE(current_time, deadline(pkg_p)))
    deliver.add_effect(at_pkg(pkg_p, l_p), False)
    deliver.add_effect(at_dest(pkg_p, l_p), True)
    p.add_action(deliver)

    # Goals
    p.add_goal(at_dest(package1, l3))
    p.add_goal(at_dest(package2, l1))
    p.add_goal(at_dest(package3, l1))

    return p
