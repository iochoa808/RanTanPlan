"""
Drone — 1×8×1 grid, 8 locations to visit, battery-constrained, instance p3.

x: IntType(0,1); y: IntType(0,8); z: IntType(0,1); battery: IntType(0,21)
xl/yl/zl: static per-location coordinate fluents.
PDDL-XTS counterpart: PDDL-XTS/drone/instances/p3.pddl
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('drone_p3')

    Location = UserType('Location')
    locs = [Object(f'x0y{y}z0', Location) for y in range(8)]
    p.add_objects(locs)

    x_t = IntType(0, 1)
    y_t = IntType(0, 8)
    z_t = IntType(0, 1)
    bat_t = IntType(0, 21)

    x   = Fluent('x',             x_t)
    y   = Fluent('y',             y_t)
    z   = Fluent('z',             z_t)
    bat = Fluent('battery_level', bat_t)
    xl  = Fluent('xl',  x_t, l=Location)
    yl  = Fluent('yl',  y_t, l=Location)
    zl  = Fluent('zl',  z_t, l=Location)
    vis = Fluent('visited', BoolType(), l=Location)

    p.add_fluent(x,   default_initial_value=0)
    p.add_fluent(y,   default_initial_value=0)
    p.add_fluent(z,   default_initial_value=0)
    p.add_fluent(bat, default_initial_value=21)
    p.add_fluent(xl,  default_initial_value=0)
    p.add_fluent(yl,  default_initial_value=0)
    p.add_fluent(zl,  default_initial_value=0)
    p.add_fluent(vis, default_initial_value=False)

    # Static coordinates
    for i, loc in enumerate(locs):
        p.set_initial_value(xl(loc), 0)
        p.set_initial_value(yl(loc), i)
        p.set_initial_value(zl(loc), 0)

    p.set_initial_value(x, 0)
    p.set_initial_value(y, 0)
    p.set_initial_value(z, 0)
    p.set_initial_value(bat, 21)

    def _move(name, fluent, lb, ub, delta):
        act = InstantaneousAction(name)
        act.add_precondition(GE(bat, 1))
        if delta > 0:
            act.add_precondition(LT(fluent, ub))
        else:
            act.add_precondition(GT(fluent, lb))
        act.add_effect(fluent, Plus(fluent, delta))
        act.add_effect(bat, Minus(bat, 1))
        p.add_action(act)

    _move('increase_x', x, 0, 1,  1)
    _move('decrease_x', x, 0, 1, -1)
    _move('increase_y', y, 0, 8,  1)
    _move('decrease_y', y, 0, 8, -1)
    _move('increase_z', z, 0, 1,  1)
    _move('decrease_z', z, 0, 1, -1)

    visit = InstantaneousAction('visit', loc=Location)
    l = visit.parameter('loc')
    visit.add_precondition(GE(bat, 1))
    visit.add_precondition(Equals(xl(l), x))
    visit.add_precondition(Equals(yl(l), y))
    visit.add_precondition(Equals(zl(l), z))
    visit.add_effect(vis(l), True)
    visit.add_effect(bat, Minus(bat, 1))
    p.add_action(visit)

    recharge = InstantaneousAction('recharge')
    recharge.add_precondition(Equals(x, 0))
    recharge.add_precondition(Equals(y, 0))
    recharge.add_precondition(Equals(z, 0))
    recharge.add_effect(bat, 21)
    p.add_action(recharge)

    for loc in locs:
        p.add_goal(vis(loc))
    p.add_goal(Equals(x, 0))
    p.add_goal(Equals(y, 0))
    p.add_goal(Equals(z, 0))

    return p
