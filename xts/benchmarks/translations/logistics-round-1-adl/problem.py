"""
logistics-round-1-adl XTS. Object fluent: (city-of ?l) - city.
Forall conditional effects on drive/fly. Expected: 13 steps.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *
from unified_planning.model import Variable


def get_problem():
    p = Problem("simple_logistics_xts")

    # Types
    PhysObj  = UserType("physobj")
    Obj      = UserType("obj",      father=PhysObj)
    Vehicle  = UserType("vehicle",  father=PhysObj)
    Truck    = UserType("truck",    father=Vehicle)
    Airplane = UserType("airplane", father=Vehicle)
    Location = UserType("location")
    City     = UserType("city")
    Airport  = UserType("airport",  father=Location)

    # Objects
    package1 = Object("package1", Obj)
    package2 = Object("package2", Obj)
    city1    = Object("city1",    City)
    city2    = Object("city2",    City)
    truck1   = Object("truck1",   Truck)
    truck2   = Object("truck2",   Truck)
    plane1   = Object("plane1",   Airplane)
    city1_1  = Object("city1-1",  Location)
    city2_1  = Object("city2-1",  Location)
    city1_2  = Object("city1-2",  Airport)
    city2_2  = Object("city2-2",  Airport)
    p.add_objects([package1, package2, city1, city2, truck1, truck2, plane1,
                   city1_1, city2_1, city1_2, city2_2])

    # Boolean predicates
    at     = Fluent("at",     x=PhysObj, l=Location)
    in_    = Fluent("in",     x=Obj,     t=Vehicle)
    loaded = Fluent("loaded", x=PhysObj)
    p.add_fluent(at,     default_initial_value=False)
    p.add_fluent(in_,    default_initial_value=False)
    p.add_fluent(loaded, default_initial_value=False)

    # Object fluent: (city-of ?l) - city
    city_of = Fluent("city-of", City, l=Location)
    p.add_fluent(city_of)

    # Initial state
    p.set_initial_value(city_of(city1_1), city1)
    p.set_initial_value(city_of(city1_2), city1)
    p.set_initial_value(city_of(city2_1), city2)
    p.set_initial_value(city_of(city2_2), city2)
    p.set_initial_value(at(plane1,    city1_2), True)
    p.set_initial_value(at(truck1,    city1_1), True)
    p.set_initial_value(at(truck2,    city2_1), True)
    p.set_initial_value(at(package1,  city1_1), True)
    p.set_initial_value(at(package2,  city2_1), True)

    # load(?obj, ?veh, ?loc)
    load = InstantaneousAction("load", obj=Obj, veh=Vehicle, loc=Location)
    obj_p, veh_p, loc_p = load.parameter("obj"), load.parameter("veh"), load.parameter("loc")
    load.add_precondition(at(obj_p, loc_p))
    load.add_precondition(at(veh_p, loc_p))
    load.add_precondition(Not(loaded(obj_p)))
    load.add_effect(in_(obj_p, veh_p), True)
    load.add_effect(loaded(obj_p), True)
    p.add_action(load)

    # unload(?obj, ?veh, ?loc)
    unload = InstantaneousAction("unload", obj=Obj, veh=Vehicle, loc=Location)
    obj_p, veh_p, loc_p = unload.parameter("obj"), unload.parameter("veh"), unload.parameter("loc")
    unload.add_precondition(in_(obj_p, veh_p))
    unload.add_precondition(at(veh_p, loc_p))
    unload.add_effect(in_(obj_p, veh_p), False)
    unload.add_effect(loaded(obj_p), False)
    p.add_action(unload)

    # drive-truck(?truck, ?from, ?to): carries all in-truck objects along
    drive = InstantaneousAction("drive-truck", truck=Truck, loc_from=Location, loc_to=Location)
    truck_p = drive.parameter("truck")
    lf_p    = drive.parameter("loc_from")
    lt_p    = drive.parameter("loc_to")
    drive.add_precondition(at(truck_p, lf_p))
    drive.add_precondition(Equals(city_of(lf_p), city_of(lt_p)))
    drive.add_effect(at(truck_p, lt_p), True)
    drive.add_effect(at(truck_p, lf_p), False)
    # forall (?x - obj) when (in ?x ?truck) -> move x too
    v = Variable("x", Obj)
    drive.add_effect(at(v, lf_p), False, condition=in_(v, truck_p), forall=[v])
    drive.add_effect(at(v, lt_p), True,  condition=in_(v, truck_p), forall=[v])
    p.add_action(drive)

    # fly-airplane(?plane, ?from, ?to): carries all on-board objects along
    fly = InstantaneousAction("fly-airplane", plane=Airplane, loc_from=Airport, loc_to=Airport)
    plane_p = fly.parameter("plane")
    lf_p    = fly.parameter("loc_from")
    lt_p    = fly.parameter("loc_to")
    fly.add_precondition(at(plane_p, lf_p))
    fly.add_effect(at(plane_p, lt_p), True)
    fly.add_effect(at(plane_p, lf_p), False)
    v2 = Variable("x", Obj)
    fly.add_effect(at(v2, lf_p), False, condition=in_(v2, plane_p), forall=[v2])
    fly.add_effect(at(v2, lt_p), True,  condition=in_(v2, plane_p), forall=[v2])
    p.add_action(fly)

    # Goal
    p.add_goal(at(package1, city2_1))
    p.add_goal(at(package2, city1_2))

    return p
