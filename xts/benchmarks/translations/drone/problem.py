"""
Drone XTS — visit two locations and return home.
XTS features: bounded integers (3D coords, battery).
Plan: 4 steps (visit x0y0z0, increase_z, visit x0y0z1, decrease_z).
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('drone_1x1x2_xts')

    Location = UserType('location')

    cx_t   = IntType(0, 1)
    cy_t   = IntType(0, 1)
    cz_t   = IntType(0, 2)
    batt_t = IntType(0, 9)

    # Boolean predicates
    visited = Fluent('visited', x=Location)
    p.add_fluent(visited, default_initial_value=False)

    # Bounded int fluents
    x_f    = Fluent('x',    cx_t)
    y_f    = Fluent('y',    cy_t)
    z_f    = Fluent('z',    cz_t)
    xl_f   = Fluent('xl',   cx_t, l=Location)
    yl_f   = Fluent('yl',   cy_t, l=Location)
    zl_f   = Fluent('zl',   cz_t, l=Location)
    batt   = Fluent('battery_level',      batt_t)
    batt_full = Fluent('battery_level_full', batt_t)
    for f in [x_f, y_f, z_f, batt, batt_full]:
        p.add_fluent(f, default_initial_value=0)
    for f in [xl_f, yl_f, zl_f]:
        p.add_fluent(f, default_initial_value=0)

    # Objects
    x0y0z0 = Object('x0y0z0', Location)
    x0y0z1 = Object('x0y0z1', Location)
    p.add_objects([x0y0z0, x0y0z1])

    # Initial state
    p.set_initial_value(x_f, Int(0))
    p.set_initial_value(y_f, Int(0))
    p.set_initial_value(z_f, Int(0))
    p.set_initial_value(xl_f(x0y0z0), Int(0))
    p.set_initial_value(yl_f(x0y0z0), Int(0))
    p.set_initial_value(zl_f(x0y0z0), Int(0))
    p.set_initial_value(xl_f(x0y0z1), Int(0))
    p.set_initial_value(yl_f(x0y0z1), Int(0))
    p.set_initial_value(zl_f(x0y0z1), Int(1))
    p.set_initial_value(batt,      Int(9))
    p.set_initial_value(batt_full, Int(9))

    # Move actions
    increase_x = InstantaneousAction('increase_x')
    increase_x.add_precondition(GE(batt, Int(1)))
    increase_x.add_precondition(LT(x_f, Int(1)))
    increase_x.add_effect(x_f, Plus(x_f, Int(1)))
    increase_x.add_effect(batt, Minus(batt, Int(1)))
    p.add_action(increase_x)

    decrease_x = InstantaneousAction('decrease_x')
    decrease_x.add_precondition(GE(batt, Int(1)))
    decrease_x.add_precondition(GT(x_f, Int(0)))
    decrease_x.add_effect(x_f, Minus(x_f, Int(1)))
    decrease_x.add_effect(batt, Minus(batt, Int(1)))
    p.add_action(decrease_x)

    increase_y = InstantaneousAction('increase_y')
    increase_y.add_precondition(GE(batt, Int(1)))
    increase_y.add_precondition(LT(y_f, Int(1)))
    increase_y.add_effect(y_f, Plus(y_f, Int(1)))
    increase_y.add_effect(batt, Minus(batt, Int(1)))
    p.add_action(increase_y)

    decrease_y = InstantaneousAction('decrease_y')
    decrease_y.add_precondition(GE(batt, Int(1)))
    decrease_y.add_precondition(GT(y_f, Int(0)))
    decrease_y.add_effect(y_f, Minus(y_f, Int(1)))
    decrease_y.add_effect(batt, Minus(batt, Int(1)))
    p.add_action(decrease_y)

    increase_z = InstantaneousAction('increase_z')
    increase_z.add_precondition(GE(batt, Int(1)))
    increase_z.add_precondition(LT(z_f, Int(2)))
    increase_z.add_effect(z_f, Plus(z_f, Int(1)))
    increase_z.add_effect(batt, Minus(batt, Int(1)))
    p.add_action(increase_z)

    decrease_z = InstantaneousAction('decrease_z')
    decrease_z.add_precondition(GE(batt, Int(1)))
    decrease_z.add_precondition(GT(z_f, Int(0)))
    decrease_z.add_effect(z_f, Minus(z_f, Int(1)))
    decrease_z.add_effect(batt, Minus(batt, Int(1)))
    p.add_action(decrease_z)

    # visit(?l)
    visit = InstantaneousAction('visit', l=Location)
    l = visit.parameter('l')
    visit.add_precondition(GE(batt, Int(1)))
    visit.add_precondition(Equals(xl_f(l), x_f))
    visit.add_precondition(Equals(yl_f(l), y_f))
    visit.add_precondition(Equals(zl_f(l), z_f))
    visit.add_effect(visited(l), True)
    visit.add_effect(batt, Minus(batt, Int(1)))
    p.add_action(visit)

    # recharge
    recharge = InstantaneousAction('recharge')
    recharge.add_precondition(Equals(x_f, Int(0)))
    recharge.add_precondition(Equals(y_f, Int(0)))
    recharge.add_precondition(Equals(z_f, Int(0)))
    recharge.add_effect(batt, batt_full)
    p.add_action(recharge)

    # Goals
    p.add_goal(visited(x0y0z0))
    p.add_goal(visited(x0y0z1))
    p.add_goal(Equals(x_f, Int(0)))
    p.add_goal(Equals(y_f, Int(0)))
    p.add_goal(Equals(z_f, Int(0)))

    return p
