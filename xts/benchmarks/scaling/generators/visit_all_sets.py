"""
Visit-All grid generator — set covering-goal encoding.

N×N grid of places (4-connected, no wrap). Robot starts at (0,0).
The covering goal uses a set fluent: (visited) accumulates visited places;
goal is (cardinality (visited)) = N*N.

This is the XTS diffViewpoints encoding: N boolean goal atoms are replaced by
a single set fluent + one cardinality equality, independent of grid size.

Compare against: visit_all_bool (boolean-predicate encoding) for compile-time overhead.
"""
import sys, os
sys.path.insert(0, os.path.expanduser('~/unified-planning'))
from unified_planning.shortcuts import *


def generate(n: int):
    """Return a visit-all UP Problem on an N×N grid with set covering goal."""
    p = Problem(f'visit_all_sets_{n}x{n}')
    Place    = UserType('place')
    PlaceSet = SetType(Place)

    locs = [[Object(f'x{x}y{y}', Place) for y in range(n)] for x in range(n)]
    flat = [locs[x][y] for x in range(n) for y in range(n)]
    p.add_objects(flat)

    robot_at = Fluent('robot_at', Place)
    connects = Fluent('connects', PlaceSet, x=Place)
    visited  = Fluent('visited', PlaceSet)
    p.add_fluent(robot_at)
    p.add_fluent(connects)
    p.add_fluent(visited)

    start = locs[0][0]
    p.set_initial_value(robot_at, start)
    p.set_initial_value(visited, {start})

    def nb(x, y):
        s = set()
        for dx, dy in [(0, 1), (0, -1), (1, 0), (-1, 0)]:
            nx, ny = x + dx, y + dy
            if 0 <= nx < n and 0 <= ny < n:
                s.add(locs[nx][ny])
        return s

    for x in range(n):
        for y in range(n):
            p.set_initial_value(connects(locs[x][y]), nb(x, y))

    move = InstantaneousAction('move', curpos=Place, nextpos=Place)
    cur, nxt = move.parameter('curpos'), move.parameter('nextpos')
    move.add_precondition(Equals(robot_at, cur))
    move.add_precondition(SetMember(nxt, connects(cur)))
    move.add_effect(robot_at, nxt)
    move.add_effect(visited, SetAdd(nxt, FluentExp(visited)))
    p.add_action(move)

    p.add_goal(Equals(SetCardinality(FluentExp(visited)), n * n))
    return p
