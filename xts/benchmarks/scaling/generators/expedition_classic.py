"""
Expedition generator — classic XTS encoding (object fluents + is_next predicate).

Linear track of N waypoints w0..w(N-1). One sled starts at w0 with 1 supply unit
and capacity N. The depot at w0 holds N supply units. The sled must retrieve
(N-2) units before walking the N-1 steps to the goal.

Features: object fluents, bounded integers, boolean predicate (is_next).
This is the PDDL-XTS translation-style encoding:
  - sled location: object fluent  (sled_at ?s) - waypoint
  - adjacency:     static predicate (is_next ?x ?y - waypoint)
  - supplies:      bounded ints

Compare against: expedition_array (1D array + bounded-int position) to see
how encoding choice affects native vs UP pipeline compile time.

Min plan length: (N-2) retrieves + (N-1) moves = 2N-3.
"""
import sys, os
sys.path.insert(0, os.path.expanduser('~/unified-planning'))
from unified_planning.shortcuts import *


def generate(n: int):
    """Return an expedition UP Problem with n waypoints (goal: reach waypoint n-1)."""
    p = Problem(f'expedition_classic_n{n}')

    Sled     = UserType('sled')
    Waypoint = UserType('waypoint')
    supply_t = IntType(0, n + 2)

    sled      = Object('s0', Sled)
    waypoints = [Object(f'w{i}', Waypoint) for i in range(n)]
    p.add_objects([sled] + waypoints)

    sled_at       = Fluent('sled_at', Waypoint, s=Sled)
    sled_supplies = Fluent('sled_supplies', supply_t, s=Sled)
    sled_capacity = Fluent('sled_capacity', supply_t, s=Sled)
    wp_supplies   = Fluent('wp_supplies', supply_t, w=Waypoint)
    is_next       = Fluent('is_next', BoolType(), x=Waypoint, y=Waypoint)

    p.add_fluent(sled_at)
    p.add_fluent(sled_supplies, default_initial_value=0)
    p.add_fluent(sled_capacity, default_initial_value=0)
    p.add_fluent(wp_supplies, default_initial_value=0)
    p.add_fluent(is_next, default_initial_value=False)

    p.set_initial_value(sled_at(sled), waypoints[0])
    p.set_initial_value(sled_supplies(sled), 1)
    p.set_initial_value(sled_capacity(sled), n)
    for w in waypoints:
        p.set_initial_value(wp_supplies(w), 0)
    p.set_initial_value(wp_supplies(waypoints[0]), n)  # depot at start
    for i in range(n - 1):
        p.set_initial_value(is_next(waypoints[i], waypoints[i + 1]), True)

    move_fwd = InstantaneousAction('move_forward', s=Sled, w1=Waypoint, w2=Waypoint)
    s, w1, w2 = [move_fwd.parameter(x) for x in ['s', 'w1', 'w2']]
    move_fwd.add_precondition(Equals(sled_at(s), w1))
    move_fwd.add_precondition(is_next(w1, w2))
    move_fwd.add_precondition(GE(sled_supplies(s), 1))
    move_fwd.add_effect(sled_at(s), w2)
    move_fwd.add_effect(sled_supplies(s), Minus(sled_supplies(s), 1))
    p.add_action(move_fwd)

    retrieve = InstantaneousAction('retrieve', s=Sled, w=Waypoint)
    s, w = retrieve.parameter('s'), retrieve.parameter('w')
    retrieve.add_precondition(Equals(sled_at(s), w))
    retrieve.add_precondition(GE(wp_supplies(w), 1))
    retrieve.add_precondition(GT(sled_capacity(s), sled_supplies(s)))
    retrieve.add_effect(wp_supplies(w), Minus(wp_supplies(w), 1))
    retrieve.add_effect(sled_supplies(s), Plus(sled_supplies(s), 1))
    p.add_action(retrieve)

    p.add_goal(Equals(sled_at(sled), waypoints[n - 1]))
    return p
