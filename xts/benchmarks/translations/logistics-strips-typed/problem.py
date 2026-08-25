"""
logistics-strips-typed XTS. Object fluent: (city-of ?p) - city.
Expected: 20 steps.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem("logistics_4_0_xts")

    # Types
    City     = UserType("city")
    Place    = UserType("place")
    PhysObj  = UserType("physobj")
    Location = UserType("location", father=Place)
    Airport  = UserType("airport",  father=Place)
    Vehicle  = UserType("vehicle",  father=PhysObj)
    Package  = UserType("package",  father=PhysObj)
    Truck    = UserType("truck",    father=Vehicle)
    Airplane = UserType("airplane", father=Vehicle)

    # Objects
    apn1  = Object("apn1",  Airplane)
    apt1  = Object("apt1",  Airport)
    apt2  = Object("apt2",  Airport)
    pos1  = Object("pos1",  Location)
    pos2  = Object("pos2",  Location)
    cit1  = Object("cit1",  City)
    cit2  = Object("cit2",  City)
    tru1  = Object("tru1",  Truck)
    tru2  = Object("tru2",  Truck)
    obj11 = Object("obj11", Package)
    obj12 = Object("obj12", Package)
    obj13 = Object("obj13", Package)
    obj21 = Object("obj21", Package)
    obj22 = Object("obj22", Package)
    obj23 = Object("obj23", Package)
    p.add_objects([apn1, apt1, apt2, pos1, pos2, cit1, cit2,
                   tru1, tru2, obj11, obj12, obj13, obj21, obj22, obj23])

    # Boolean predicates
    at = Fluent("at", obj=PhysObj, loc=Place)
    in_ = Fluent("in", pkg=Package, veh=Vehicle)
    p.add_fluent(at,  default_initial_value=False)
    p.add_fluent(in_, default_initial_value=False)

    # Object fluent: (city-of ?p) - city
    city_of = Fluent("city-of", City, p=Place)
    p.add_fluent(city_of)

    # Initial state
    p.set_initial_value(at(apn1, apt2),   True)
    p.set_initial_value(at(tru1, pos1),   True)
    p.set_initial_value(at(obj11, pos1),  True)
    p.set_initial_value(at(obj12, pos1),  True)
    p.set_initial_value(at(obj13, pos1),  True)
    p.set_initial_value(at(tru2, pos2),   True)
    p.set_initial_value(at(obj21, pos2),  True)
    p.set_initial_value(at(obj22, pos2),  True)
    p.set_initial_value(at(obj23, pos2),  True)
    p.set_initial_value(city_of(pos1), cit1)
    p.set_initial_value(city_of(apt1), cit1)
    p.set_initial_value(city_of(pos2), cit2)
    p.set_initial_value(city_of(apt2), cit2)

    # LOAD-TRUCK(?pkg, ?truck, ?loc)
    load_truck = InstantaneousAction("LOAD-TRUCK", pkg=Package, truck=Truck, loc=Place)
    pkg_p, truck_p, loc_p = load_truck.parameter("pkg"), load_truck.parameter("truck"), load_truck.parameter("loc")
    load_truck.add_precondition(at(truck_p, loc_p))
    load_truck.add_precondition(at(pkg_p, loc_p))
    load_truck.add_effect(at(pkg_p, loc_p), False)
    load_truck.add_effect(in_(pkg_p, truck_p), True)
    p.add_action(load_truck)

    # LOAD-AIRPLANE(?pkg, ?airplane, ?loc)
    load_plane = InstantaneousAction("LOAD-AIRPLANE", pkg=Package, airplane=Airplane, loc=Place)
    pkg_p, plane_p, loc_p = load_plane.parameter("pkg"), load_plane.parameter("airplane"), load_plane.parameter("loc")
    load_plane.add_precondition(at(pkg_p, loc_p))
    load_plane.add_precondition(at(plane_p, loc_p))
    load_plane.add_effect(at(pkg_p, loc_p), False)
    load_plane.add_effect(in_(pkg_p, plane_p), True)
    p.add_action(load_plane)

    # UNLOAD-TRUCK(?pkg, ?truck, ?loc)
    unload_truck = InstantaneousAction("UNLOAD-TRUCK", pkg=Package, truck=Truck, loc=Place)
    pkg_p, truck_p, loc_p = unload_truck.parameter("pkg"), unload_truck.parameter("truck"), unload_truck.parameter("loc")
    unload_truck.add_precondition(at(truck_p, loc_p))
    unload_truck.add_precondition(in_(pkg_p, truck_p))
    unload_truck.add_effect(in_(pkg_p, truck_p), False)
    unload_truck.add_effect(at(pkg_p, loc_p), True)
    p.add_action(unload_truck)

    # UNLOAD-AIRPLANE(?pkg, ?airplane, ?loc)
    unload_plane = InstantaneousAction("UNLOAD-AIRPLANE", pkg=Package, airplane=Airplane, loc=Place)
    pkg_p, plane_p, loc_p = unload_plane.parameter("pkg"), unload_plane.parameter("airplane"), unload_plane.parameter("loc")
    unload_plane.add_precondition(in_(pkg_p, plane_p))
    unload_plane.add_precondition(at(plane_p, loc_p))
    unload_plane.add_effect(in_(pkg_p, plane_p), False)
    unload_plane.add_effect(at(pkg_p, loc_p), True)
    p.add_action(unload_plane)

    # DRIVE-TRUCK(?truck, ?from, ?to)
    drive = InstantaneousAction("DRIVE-TRUCK", truck=Truck, loc_from=Place, loc_to=Place)
    truck_p = drive.parameter("truck")
    lf_p    = drive.parameter("loc_from")
    lt_p    = drive.parameter("loc_to")
    drive.add_precondition(at(truck_p, lf_p))
    drive.add_precondition(Equals(city_of(lf_p), city_of(lt_p)))
    drive.add_effect(at(truck_p, lf_p), False)
    drive.add_effect(at(truck_p, lt_p), True)
    p.add_action(drive)

    # FLY-AIRPLANE(?airplane, ?from, ?to)
    fly = InstantaneousAction("FLY-AIRPLANE", airplane=Airplane, loc_from=Airport, loc_to=Airport)
    plane_p = fly.parameter("airplane")
    lf_p    = fly.parameter("loc_from")
    lt_p    = fly.parameter("loc_to")
    fly.add_precondition(at(plane_p, lf_p))
    fly.add_effect(at(plane_p, lf_p), False)
    fly.add_effect(at(plane_p, lt_p), True)
    p.add_action(fly)

    # Goal
    p.add_goal(at(obj11, apt1))
    p.add_goal(at(obj23, pos1))
    p.add_goal(at(obj13, apt1))
    p.add_goal(at(obj21, pos1))

    return p
