"""
hiking-sequential-optimal XTS. Four object fluents: tent-at, person-at, car-at, walked-at.
Expected: 11 steps.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem("Hiking_1_2_xts")

    Car    = UserType("car")
    Tent   = UserType("tent")
    Person = UserType("person")
    Couple = UserType("couple")
    Place  = UserType("place")

    # Objects
    car0   = Object("car0",   Car)
    car1   = Object("car1",   Car)
    tent0  = Object("tent0",  Tent)
    couple0= Object("couple0", Couple)
    place0 = Object("place0", Place)
    place1 = Object("place1", Place)
    place2 = Object("place2", Place)
    guy0   = Object("guy0",   Person)
    girl0  = Object("girl0",  Person)
    p.add_objects([car0, car1, tent0, couple0, place0, place1, place2, guy0, girl0])

    # Boolean predicates
    partners = Fluent("partners", x1=Couple, x2=Person, x3=Person)
    up       = Fluent("up",       x1=Tent)
    down     = Fluent("down",     x1=Tent)
    next_pl  = Fluent("next",     x1=Place,  x2=Place)
    for f in [partners, up, down, next_pl]:
        p.add_fluent(f, default_initial_value=False)

    # Object fluents
    tent_at   = Fluent("tent-at",   Place, t=Tent)
    person_at = Fluent("person-at", Place, person=Person)
    car_at    = Fluent("car-at",    Place, c=Car)
    walked_at = Fluent("walked-at", Place, cp=Couple)
    for f in [tent_at, person_at, car_at, walked_at]:
        p.add_fluent(f)

    # Initial state
    p.set_initial_value(partners(couple0, guy0, girl0), True)
    p.set_initial_value(person_at(guy0),   place0)
    p.set_initial_value(person_at(girl0),  place0)
    p.set_initial_value(walked_at(couple0),place0)
    p.set_initial_value(tent_at(tent0),    place0)
    p.set_initial_value(up(tent0),         True)
    p.set_initial_value(car_at(car0),      place0)
    p.set_initial_value(car_at(car1),      place0)
    p.set_initial_value(next_pl(place0, place1), True)
    p.set_initial_value(next_pl(place1, place2), True)

    # put_down(?x1 - person, ?x3 - tent)
    put_down = InstantaneousAction("put_down", x1=Person, x3=Tent)
    x1_p, x3_p = put_down.parameter("x1"), put_down.parameter("x3")
    put_down.add_precondition(Equals(person_at(x1_p), tent_at(x3_p)))
    put_down.add_precondition(up(x3_p))
    put_down.add_effect(down(x3_p), True)
    put_down.add_effect(up(x3_p), False)
    p.add_action(put_down)

    # put_up(?x1 - person, ?x3 - tent)
    put_up = InstantaneousAction("put_up", x1=Person, x3=Tent)
    x1_p, x3_p = put_up.parameter("x1"), put_up.parameter("x3")
    put_up.add_precondition(Equals(person_at(x1_p), tent_at(x3_p)))
    put_up.add_precondition(down(x3_p))
    put_up.add_effect(up(x3_p), True)
    put_up.add_effect(down(x3_p), False)
    p.add_action(put_up)

    # drive(?x1 - person, ?x3 - place, ?x4 - car)
    drive = InstantaneousAction("drive", x1=Person, x3=Place, x4=Car)
    x1_p, x3_p, x4_p = drive.parameter("x1"), drive.parameter("x3"), drive.parameter("x4")
    drive.add_precondition(Equals(person_at(x1_p), car_at(x4_p)))
    drive.add_effect(person_at(x1_p), x3_p)
    drive.add_effect(car_at(x4_p),    x3_p)
    p.add_action(drive)

    # drive_passenger(?x1, ?x3, ?x4, ?x5)
    drive_pass = InstantaneousAction("drive_passenger", x1=Person, x3=Place, x4=Car, x5=Person)
    x1_p, x3_p, x4_p, x5_p = (drive_pass.parameter(n) for n in ("x1", "x3", "x4", "x5"))
    drive_pass.add_precondition(Equals(person_at(x1_p), car_at(x4_p)))
    drive_pass.add_precondition(Equals(person_at(x5_p), car_at(x4_p)))
    drive_pass.add_precondition(Not(Equals(x1_p, x5_p)))
    drive_pass.add_effect(person_at(x1_p), x3_p)
    drive_pass.add_effect(car_at(x4_p),    x3_p)
    drive_pass.add_effect(person_at(x5_p), x3_p)
    p.add_action(drive_pass)

    # drive_tent(?x1, ?x3, ?x4, ?x5)
    drive_tent = InstantaneousAction("drive_tent", x1=Person, x3=Place, x4=Car, x5=Tent)
    x1_p, x3_p, x4_p, x5_p = (drive_tent.parameter(n) for n in ("x1", "x3", "x4", "x5"))
    drive_tent.add_precondition(Equals(person_at(x1_p), car_at(x4_p)))
    drive_tent.add_precondition(Equals(tent_at(x5_p), car_at(x4_p)))
    drive_tent.add_precondition(down(x5_p))
    drive_tent.add_effect(person_at(x1_p), x3_p)
    drive_tent.add_effect(car_at(x4_p),    x3_p)
    drive_tent.add_effect(tent_at(x5_p),   x3_p)
    p.add_action(drive_tent)

    # drive_tent_passenger(?x1, ?x3, ?x4, ?x5, ?x6)
    dtp = InstantaneousAction("drive_tent_passenger", x1=Person, x3=Place, x4=Car, x5=Tent, x6=Person)
    x1_p, x3_p, x4_p, x5_p, x6_p = (dtp.parameter(n) for n in ("x1", "x3", "x4", "x5", "x6"))
    dtp.add_precondition(Equals(person_at(x1_p), car_at(x4_p)))
    dtp.add_precondition(Equals(tent_at(x5_p), car_at(x4_p)))
    dtp.add_precondition(down(x5_p))
    dtp.add_precondition(Equals(person_at(x6_p), car_at(x4_p)))
    dtp.add_precondition(Not(Equals(x1_p, x6_p)))
    dtp.add_effect(person_at(x1_p), x3_p)
    dtp.add_effect(car_at(x4_p),    x3_p)
    dtp.add_effect(tent_at(x5_p),   x3_p)
    dtp.add_effect(person_at(x6_p), x3_p)
    p.add_action(dtp)

    # walk_together(?x1-tent, ?x2-place, ?x3-person, ?x4-place, ?x5-person, ?x6-couple)
    wt = InstantaneousAction("walk_together", x1=Tent, x2=Place, x3=Person, x4=Place, x5=Person, x6=Couple)
    x1_p, x2_p, x3_p, x4_p, x5_p, x6_p = (wt.parameter(n) for n in ("x1", "x2", "x3", "x4", "x5", "x6"))
    wt.add_precondition(Equals(tent_at(x1_p), x2_p))
    wt.add_precondition(up(x1_p))
    wt.add_precondition(Equals(person_at(x3_p), x4_p))
    wt.add_precondition(next_pl(x4_p, x2_p))
    wt.add_precondition(Equals(person_at(x5_p), x4_p))
    wt.add_precondition(Not(Equals(x3_p, x5_p)))
    wt.add_precondition(Equals(walked_at(x6_p), x4_p))
    wt.add_precondition(partners(x6_p, x3_p, x5_p))
    wt.add_effect(person_at(x3_p), x2_p)
    wt.add_effect(person_at(x5_p), x2_p)
    wt.add_effect(walked_at(x6_p), x2_p)
    p.add_action(wt)

    # Goal
    p.add_goal(Equals(walked_at(couple0), place2))

    return p
