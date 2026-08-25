"""
City-Car XTS — minimal-citycar.
Features: set fluents (line-neighbors, diag-neighbors), forall/when in effect.
Plan: build_straight j1->j2, car_start j1, move_in, move_out, car_arrived  (5 steps).
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *
from unified_planning.model import Variable


def get_problem():
    p = Problem('minimal-citycar-xts')

    Car      = UserType('car')
    Junction = UserType('junction')
    Garage   = UserType('garage')
    Road     = UserType('road')

    JunctionSet = SetType(Junction)

    # Objects
    car1    = Object('car1',    Car)
    j1      = Object('j1',      Junction)
    j2      = Object('j2',      Junction)
    garage1 = Object('garage1', Garage)
    road1   = Object('road1',   Road)

    p.add_objects([car1, j1, j2, garage1, road1])

    # Fluents
    line_neighbors = Fluent('line-neighbors', JunctionSet, j=Junction)
    diag_neighbors = Fluent('diag-neighbors', JunctionSet, j=Junction)

    at_car_jun   = Fluent('at_car_jun',    c=Car, x=Junction)
    at_car_road  = Fluent('at_car_road',   c=Car, x=Road)
    starting     = Fluent('starting',      c=Car, x=Garage)
    arrived      = Fluent('arrived',       c=Car, x=Junction)
    road_connect = Fluent('road_connect',  r1=Road, xy=Junction, xy2=Junction)
    clear        = Fluent('clear',         xy=Junction)
    in_place     = Fluent('in_place',      x=Road)
    at_garage    = Fluent('at_garage',     g=Garage, xy=Junction)

    p.add_fluent(line_neighbors, default_initial_value=set())
    p.add_fluent(diag_neighbors, default_initial_value=set())
    p.add_fluent(at_car_jun,   default_initial_value=False)
    p.add_fluent(at_car_road,  default_initial_value=False)
    p.add_fluent(starting,     default_initial_value=False)
    p.add_fluent(arrived,      default_initial_value=False)
    p.add_fluent(road_connect, default_initial_value=False)
    p.add_fluent(clear,        default_initial_value=False)
    p.add_fluent(in_place,     default_initial_value=False)
    p.add_fluent(at_garage,    default_initial_value=False)

    # Init
    p.set_initial_value(line_neighbors(j1), {j2})
    p.set_initial_value(line_neighbors(j2), set())
    p.set_initial_value(diag_neighbors(j1), set())
    p.set_initial_value(diag_neighbors(j2), set())

    p.set_initial_value(clear(j1), True)
    p.set_initial_value(clear(j2), True)
    p.set_initial_value(at_garage(garage1, j1), True)
    p.set_initial_value(starting(car1, garage1), True)

    # ---- Actions ----

    move_car_in_road = InstantaneousAction('move_car_in_road',
                                           xy_initial=Junction, xy_final=Junction,
                                           machine=Car, r1=Road)
    xy_i, xy_f, mac, r1_p = [move_car_in_road.parameter(x) for x in
                               ('xy_initial', 'xy_final', 'machine', 'r1')]
    move_car_in_road.add_precondition(at_car_jun(mac, xy_i))
    move_car_in_road.add_precondition(road_connect(r1_p, xy_i, xy_f))
    move_car_in_road.add_precondition(in_place(r1_p))
    move_car_in_road.add_effect(clear(xy_i),           True)
    move_car_in_road.add_effect(at_car_road(mac, r1_p), True)
    move_car_in_road.add_effect(at_car_jun(mac, xy_i),  False)
    p.add_action(move_car_in_road)

    move_car_out_road = InstantaneousAction('move_car_out_road',
                                            xy_initial=Junction, xy_final=Junction,
                                            machine=Car, r1=Road)
    xy_i, xy_f, mac, r1_p = [move_car_out_road.parameter(x) for x in
                               ('xy_initial', 'xy_final', 'machine', 'r1')]
    move_car_out_road.add_precondition(at_car_road(mac, r1_p))
    move_car_out_road.add_precondition(clear(xy_f))
    move_car_out_road.add_precondition(road_connect(r1_p, xy_i, xy_f))
    move_car_out_road.add_precondition(in_place(r1_p))
    move_car_out_road.add_effect(at_car_jun(mac, xy_f),  True)
    move_car_out_road.add_effect(clear(xy_f),            False)
    move_car_out_road.add_effect(at_car_road(mac, r1_p), False)
    p.add_action(move_car_out_road)

    car_arrived = InstantaneousAction('car_arrived', xy_final=Junction, machine=Car)
    xy_f, mac = car_arrived.parameter('xy_final'), car_arrived.parameter('machine')
    car_arrived.add_precondition(at_car_jun(mac, xy_f))
    car_arrived.add_effect(clear(xy_f),          True)
    car_arrived.add_effect(arrived(mac, xy_f),   True)
    car_arrived.add_effect(at_car_jun(mac, xy_f), False)
    p.add_action(car_arrived)

    car_start = InstantaneousAction('car_start', xy_final=Junction, machine=Car, g=Garage)
    xy_f, mac, g_p = [car_start.parameter(x) for x in ('xy_final', 'machine', 'g')]
    car_start.add_precondition(at_garage(g_p, xy_f))
    car_start.add_precondition(starting(mac, g_p))
    car_start.add_precondition(clear(xy_f))
    car_start.add_effect(clear(xy_f),         False)
    car_start.add_effect(at_car_jun(mac, xy_f), True)
    car_start.add_effect(starting(mac, g_p),  False)
    p.add_action(car_start)

    build_diagonal_oneway = InstantaneousAction('build_diagonal_oneway',
                                                xy_initial=Junction, xy_final=Junction, r1=Road)
    xy_i, xy_f, r1_p = [build_diagonal_oneway.parameter(x) for x in ('xy_initial', 'xy_final', 'r1')]
    build_diagonal_oneway.add_precondition(clear(xy_f))
    build_diagonal_oneway.add_precondition(Not(in_place(r1_p)))
    build_diagonal_oneway.add_precondition(SetMember(xy_f, diag_neighbors(xy_i)))
    build_diagonal_oneway.add_effect(road_connect(r1_p, xy_i, xy_f), True)
    build_diagonal_oneway.add_effect(in_place(r1_p), True)
    p.add_action(build_diagonal_oneway)

    build_straight_oneway = InstantaneousAction('build_straight_oneway',
                                                xy_initial=Junction, xy_final=Junction, r1=Road)
    xy_i, xy_f, r1_p = [build_straight_oneway.parameter(x) for x in ('xy_initial', 'xy_final', 'r1')]
    build_straight_oneway.add_precondition(clear(xy_f))
    build_straight_oneway.add_precondition(Not(in_place(r1_p)))
    build_straight_oneway.add_precondition(SetMember(xy_f, line_neighbors(xy_i)))
    build_straight_oneway.add_effect(road_connect(r1_p, xy_i, xy_f), True)
    build_straight_oneway.add_effect(in_place(r1_p), True)
    p.add_action(build_straight_oneway)

    # destroy_road: forall(?c1 - car) when(at_car_road ?c1 ?r1) -> not + teleport to xy_initial
    destroy_road = InstantaneousAction('destroy_road',
                                       xy_initial=Junction, xy_final=Junction, r1=Road)
    xy_i, xy_f, r1_p = [destroy_road.parameter(x) for x in ('xy_initial', 'xy_final', 'r1')]
    destroy_road.add_precondition(road_connect(r1_p, xy_i, xy_f))
    destroy_road.add_precondition(in_place(r1_p))
    destroy_road.add_effect(in_place(r1_p), False)
    destroy_road.add_effect(road_connect(r1_p, xy_i, xy_f), False)
    c1v = Variable('c1', Car)
    destroy_road.add_effect(at_car_road(c1v, r1_p), False,
                             condition=at_car_road(c1v, r1_p), forall=[c1v])
    destroy_road.add_effect(at_car_jun(c1v, xy_i), True,
                             condition=at_car_road(c1v, r1_p), forall=[c1v])
    p.add_action(destroy_road)

    # Goal
    p.add_goal(arrived(car1, j2))

    return p
