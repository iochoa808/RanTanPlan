"""
Visit-All (grid-2): 2x2 grid, robot starts at loc-x1-y1, must visit all 4 places.
XTS features: object fluent (robot-at), set fluent (visited, connects).
PDDL-XTS domain: xts/benchmarks/translations/visit-all-sequential-optimal/domain.pddl
Plan length: 3 steps.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('grid_2_xts')

    Place = UserType('place')

    loc_x0_y0 = Object('loc-x0-y0', Place)
    loc_x0_y1 = Object('loc-x0-y1', Place)
    loc_x1_y0 = Object('loc-x1-y0', Place)
    loc_x1_y1 = Object('loc-x1-y1', Place)
    p.add_objects([loc_x0_y0, loc_x0_y1, loc_x1_y0, loc_x1_y1])

    # Object fluent: (robot-at) - place  (0-param, total function)
    robot_at = Fluent('robot_at', Place)
    p.add_fluent(robot_at)  # no default - must init explicitly

    # Set fluent: (visited) - placeset  (0-param)
    visited = Fluent('visited', SetType(Place))
    p.add_fluent(visited, default_initial_value=set())

    # Set fluent: (connects ?x) - placeset
    connects = Fluent('connects', SetType(Place), x=Place)
    p.add_fluent(connects, default_initial_value=set())

    # Initial state
    p.set_initial_value(robot_at, loc_x1_y1)
    p.set_initial_value(visited, {loc_x1_y1})
    p.set_initial_value(connects(loc_x0_y0), {loc_x1_y0, loc_x0_y1})
    p.set_initial_value(connects(loc_x0_y1), {loc_x1_y1, loc_x0_y0})
    p.set_initial_value(connects(loc_x1_y0), {loc_x0_y0, loc_x1_y1})
    p.set_initial_value(connects(loc_x1_y1), {loc_x0_y1, loc_x1_y0})

    # Action: move(?curpos, ?nextpos)
    move = InstantaneousAction('move', curpos=Place, nextpos=Place)
    curpos, nextpos = move.parameter('curpos'), move.parameter('nextpos')
    move.add_precondition(Equals(robot_at, curpos))
    move.add_precondition(SetMember(nextpos, connects(curpos)))
    move.add_effect(robot_at, nextpos)
    move.add_effect(visited, SetAdd(nextpos, visited))
    p.add_action(move)

    # Goal: all 4 places visited
    p.add_goal(SetMember(loc_x0_y0, visited))
    p.add_goal(SetMember(loc_x0_y1, visited))
    p.add_goal(SetMember(loc_x1_y0, visited))
    p.add_goal(SetMember(loc_x1_y1, visited))

    return p
