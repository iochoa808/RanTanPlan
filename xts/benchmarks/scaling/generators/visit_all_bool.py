"""
Visit-All grid generator — classic boolean-predicate encoding.

N×N grid of places (4-connected, no wrap). Robot starts at (0,0).
Classic encoding: (visited ?wp) boolean predicate per place; goal has N*N atoms.

Compare against: visit_all_sets (set covering-goal encoding) to see:
  - Compile overhead: classic PDDL needs no XTS transformation (UP pipeline fast),
    XTS sets need SETS_REMOVING (moderate overhead for native path compile).
  - Solve difference: N boolean goal atoms vs 1 cardinality equality.

The ONLY encoding difference vs visit_all_sets:
  - This:       (visited ?wp - place) boolean + N goal atoms
  - visit_all_sets: (visited) - placeset set fluent + 1 cardinality goal
Both use object fluent (robot-at) and static set (connects) for adjacency.
"""
import sys, os
sys.path.insert(0, os.path.expanduser('~/unified-planning'))
from unified_planning.shortcuts import *


def generate(n: int):
    """Return a visit-all UP Problem on an N×N grid with boolean visited predicates."""
    p = Problem(f'visit_all_bool_{n}x{n}')
    Place    = UserType('place')
    PlaceSet = SetType(Place)   # used only for the static connects adjacency set

    locs = [[Object(f'x{x}y{y}', Place) for y in range(n)] for x in range(n)]
    flat = [locs[x][y] for x in range(n) for y in range(n)]
    p.add_objects(flat)

    robot_at = Fluent('robot_at', Place)              # object fluent (same as sets version)
    connects = Fluent('connects', PlaceSet, x=Place)  # static adjacency set (same)
    visited  = Fluent('visited', BoolType(), wp=Place) # ← BOOLEAN predicate, one per place
    p.add_fluent(robot_at)
    p.add_fluent(connects)
    p.add_fluent(visited, default_initial_value=False)

    start = locs[0][0]
    p.set_initial_value(robot_at, start)
    p.set_initial_value(visited(start), True)  # starting place already visited

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
    move.add_effect(visited(nxt), True)   # ← boolean effect, one per step
    p.add_action(move)

    # Goal: N*N boolean atoms (one per place) ← grows linearly with grid size
    for wp in flat:
        p.add_goal(visited(wp))
    return p
