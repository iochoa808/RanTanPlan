"""
parking-sequential-optimal XTS. Object fluent: (parked-on ?car) - support.
supertype support = car | curb. Expected: 3 steps.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem("minimal_parking_xts")

    Support = UserType("support")
    Car     = UserType("car",  father=Support)
    Curb    = UserType("curb", father=Support)

    car1  = Object("car1",  Car)
    car2  = Object("car2",  Car)
    curb1 = Object("curb1", Curb)
    curb2 = Object("curb2", Curb)
    curb3 = Object("curb3", Curb)
    p.add_objects([car1, car2, curb1, curb2, curb3])

    # Boolean: (clear ?s)
    clear = Fluent("clear", s=Support)
    p.add_fluent(clear, default_initial_value=False)

    # Object fluent: (parked-on ?car) - support
    parked_on = Fluent("parked-on", Support, car=Car)
    p.add_fluent(parked_on)

    # Initial state
    p.set_initial_value(parked_on(car1), curb1)
    p.set_initial_value(parked_on(car2), curb2)
    p.set_initial_value(clear(car1),  True)
    p.set_initial_value(clear(car2),  True)
    p.set_initial_value(clear(curb3), True)

    # move(?car, ?src, ?dest)
    move = InstantaneousAction("move", car=Car, src=Support, dest=Support)
    car_p  = move.parameter("car")
    src_p  = move.parameter("src")
    dest_p = move.parameter("dest")
    move.add_precondition(clear(car_p))
    move.add_precondition(clear(dest_p))
    move.add_precondition(Equals(parked_on(car_p), src_p))
    move.add_precondition(Not(Equals(car_p, dest_p)))
    move.add_effect(parked_on(car_p), dest_p)
    move.add_effect(clear(src_p),  True)
    move.add_effect(clear(dest_p), False)
    p.add_action(move)

    # Goal: car1 on curb2, car2 on curb1
    p.add_goal(Equals(parked_on(car1), curb2))
    p.add_goal(Equals(parked_on(car2), curb1))

    return p
