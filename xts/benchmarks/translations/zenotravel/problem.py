"""
Zenotravel XTS — ZTRAVEL-1-2.
Features: mixed numeric (IntType 0-99999) + bounded int (onboard), object fluent (aircraft-at).
Plan: ~9 steps.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('ZTRAVEL-1-2-xts')

    Aircraft = UserType('aircraft')
    Person   = UserType('person')
    City     = UserType('city')

    obd_t  = IntType(0, 8)   # onboard count — genuinely discrete, small range
    num_t  = RealType()      # fuel / distance / burn-rates are continuous numeric
                             # quantities, not bounded counters.  IntType(0, 99999)
                             # would force INTEGERS_REMOVING to enumerate 100 k
                             # CP-SAT combinations, making the pipeline infeasible.

    # Objects
    plane1  = Object('plane1',  Aircraft)
    person1 = Object('person1', Person)
    person2 = Object('person2', Person)
    person3 = Object('person3', Person)
    city0   = Object('city0',   City)
    city1   = Object('city1',   City)
    city2   = Object('city2',   City)

    p.add_objects([plane1, person1, person2, person3, city0, city1, city2])

    # Fluents
    at_person   = Fluent('at',          p=Person,   c=City)
    in_person   = Fluent('in',          p=Person,   a=Aircraft)
    aircraft_at = Fluent('aircraft-at', City,       a=Aircraft)
    fuel        = Fluent('fuel',        num_t,      a=Aircraft)
    distance    = Fluent('distance',    num_t,      c1=City, c2=City)
    slow_burn   = Fluent('slow-burn',   num_t,      a=Aircraft)
    fast_burn   = Fluent('fast-burn',   num_t,      a=Aircraft)
    capacity    = Fluent('capacity',    num_t,      a=Aircraft)
    onboard     = Fluent('onboard',     obd_t,      a=Aircraft)
    zoom_limit  = Fluent('zoom-limit',  num_t,      a=Aircraft)

    p.add_fluent(at_person,   default_initial_value=False)
    p.add_fluent(in_person,   default_initial_value=False)
    p.add_fluent(aircraft_at, default_initial_value=city0)
    p.add_fluent(fuel,        default_initial_value=Int(0))
    p.add_fluent(distance,    default_initial_value=Int(0))
    p.add_fluent(slow_burn,   default_initial_value=Int(0))
    p.add_fluent(fast_burn,   default_initial_value=Int(0))
    p.add_fluent(capacity,    default_initial_value=Int(0))
    p.add_fluent(onboard,     default_initial_value=Int(0))
    p.add_fluent(zoom_limit,  default_initial_value=Int(0))

    # Init
    p.set_initial_value(aircraft_at(plane1), city0)
    p.set_initial_value(capacity(plane1),    Int(6000))
    p.set_initial_value(fuel(plane1),        Int(4000))
    p.set_initial_value(slow_burn(plane1),   Int(4))
    p.set_initial_value(fast_burn(plane1),   Int(15))
    p.set_initial_value(zoom_limit(plane1),  Int(8))
    p.set_initial_value(onboard(plane1),     Int(0))

    p.set_initial_value(at_person(person1, city0), True)
    p.set_initial_value(at_person(person2, city0), True)
    p.set_initial_value(at_person(person3, city1), True)

    p.set_initial_value(distance(city0, city0), Int(0))
    p.set_initial_value(distance(city0, city1), Int(678))
    p.set_initial_value(distance(city0, city2), Int(775))
    p.set_initial_value(distance(city1, city0), Int(678))
    p.set_initial_value(distance(city1, city1), Int(0))
    p.set_initial_value(distance(city1, city2), Int(810))
    p.set_initial_value(distance(city2, city0), Int(775))
    p.set_initial_value(distance(city2, city1), Int(810))
    p.set_initial_value(distance(city2, city2), Int(0))

    # ---- Actions ----

    board = InstantaneousAction('board', pers=Person, a=Aircraft, c=City)
    pers_p, a_p, c_p = [board.parameter(x) for x in ('pers', 'a', 'c')]
    board.add_precondition(at_person(pers_p, c_p))
    board.add_precondition(Equals(aircraft_at(a_p), c_p))
    board.add_effect(at_person(pers_p, c_p), False)
    board.add_effect(in_person(pers_p, a_p), True)
    board.add_effect(onboard(a_p), Plus(onboard(a_p), Int(1)))
    p.add_action(board)

    debark = InstantaneousAction('debark', pers=Person, a=Aircraft, c=City)
    pers_p, a_p, c_p = [debark.parameter(x) for x in ('pers', 'a', 'c')]
    debark.add_precondition(in_person(pers_p, a_p))
    debark.add_precondition(Equals(aircraft_at(a_p), c_p))
    debark.add_effect(in_person(pers_p, a_p),  False)
    debark.add_effect(at_person(pers_p, c_p),  True)
    debark.add_effect(onboard(a_p), Minus(onboard(a_p), Int(1)))
    p.add_action(debark)

    fly_slow = InstantaneousAction('fly-slow', a=Aircraft, c1=City, c2=City)
    a_p, c1_p, c2_p = [fly_slow.parameter(x) for x in ('a', 'c1', 'c2')]
    fly_slow.add_precondition(Equals(aircraft_at(a_p), c1_p))
    fly_slow.add_precondition(Not(Equals(c1_p, c2_p)))
    fly_slow.add_precondition(GE(fuel(a_p), Times(distance(c1_p, c2_p), slow_burn(a_p))))
    fly_slow.add_effect(aircraft_at(a_p), c2_p)
    fly_slow.add_effect(fuel(a_p), Minus(fuel(a_p), Times(distance(c1_p, c2_p), slow_burn(a_p))))
    p.add_action(fly_slow)

    fly_fast = InstantaneousAction('fly-fast', a=Aircraft, c1=City, c2=City)
    a_p, c1_p, c2_p = [fly_fast.parameter(x) for x in ('a', 'c1', 'c2')]
    fly_fast.add_precondition(Equals(aircraft_at(a_p), c1_p))
    fly_fast.add_precondition(Not(Equals(c1_p, c2_p)))
    fly_fast.add_precondition(GE(fuel(a_p), Times(distance(c1_p, c2_p), fast_burn(a_p))))
    fly_fast.add_precondition(LE(onboard(a_p), zoom_limit(a_p)))
    fly_fast.add_effect(aircraft_at(a_p), c2_p)
    fly_fast.add_effect(fuel(a_p), Minus(fuel(a_p), Times(distance(c1_p, c2_p), fast_burn(a_p))))
    p.add_action(fly_fast)

    refuel = InstantaneousAction('refuel', a=Aircraft)
    a_p = refuel.parameter('a')
    refuel.add_precondition(GT(capacity(a_p), fuel(a_p)))
    refuel.add_effect(fuel(a_p), capacity(a_p))
    p.add_action(refuel)

    # Goals
    p.add_goal(at_person(person1, city2))
    p.add_goal(at_person(person2, city1))
    p.add_goal(at_person(person3, city2))

    return p
