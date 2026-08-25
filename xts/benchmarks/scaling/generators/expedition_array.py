"""
Expedition generator — XTS diffViewpoints encoding (1D array + bounded-int position).

Same planning problem as expedition_classic but using a 1D array for track supplies
and a bounded-int fluent for sled position — eliminating the is_next predicate and
all waypoint objects.

Features: arrays, bounded integers.
This is the PDDL-XTS diffViewpoints encoding from diffViewpoints/expedition/:
  - sled position:   bounded-int fluent (sled_pos ?s) - pos ∈ [0, N-1]
  - track supplies:  1D array (track_supplies) : array[N, IntType(0, N+2)]
  - adjacency:       implicit via ±1 arithmetic on pos (no is_next predicate needed)
  - value-binding:   (= (read track_supplies ?p) ?v) binds cell value to parameter
  - parameter-guard: (= (sled_pos ?s) ?p) binds position to parameter ?p used as index

Compare against: expedition_classic (object fluents + is_next predicate) to see
how the array encoding changes compile time for both native and UP pipeline paths.

Min plan length: (N-2) retrieves + (N-1) moves = 2N-3.
"""
import sys, os
sys.path.insert(0, os.path.expanduser('~/unified-planning'))
from unified_planning.shortcuts import *


def generate(n: int):
    """Return an expedition UP Problem with n positions (goal: reach position n-1)."""
    p = Problem(f'expedition_array_n{n}')

    Sled     = UserType('sled')
    pos_t    = IntType(0, n - 1)
    supply_t = IntType(0, n + 2)
    track_t  = ArrayType(n, supply_t)

    sled = Object('s0', Sled)
    p.add_objects([sled])

    track    = Fluent('track_supplies', track_t)
    sled_pos = Fluent('sled_pos', pos_t, s=Sled)
    sled_sup = Fluent('sled_supplies', supply_t, s=Sled)
    sled_cap = Fluent('sled_capacity', supply_t, s=Sled)
    p.add_fluent(track)
    p.add_fluent(sled_pos)
    p.add_fluent(sled_sup, default_initial_value=0)
    p.add_fluent(sled_cap, default_initial_value=0)

    # depot at position 0, sled starts there with 1 supply
    p.set_initial_value(track, [n] + [0] * (n - 1))
    p.set_initial_value(sled_pos(sled), 0)
    p.set_initial_value(sled_sup(sled), 1)
    p.set_initial_value(sled_cap(sled), n)

    # move_forward: parameter-guard (= (sled_pos ?s) ?p) + arithmetic
    move_fwd = InstantaneousAction('move_forward', s=Sled, p=pos_t)
    s, ppar = move_fwd.parameter('s'), move_fwd.parameter('p')
    move_fwd.add_precondition(Equals(sled_pos(s), ppar))
    move_fwd.add_precondition(LT(ppar, n - 1))   # boundary guard: p < n-1
    move_fwd.add_precondition(GE(sled_sup(s), 1))
    move_fwd.add_effect(sled_pos(s), Plus(ppar, 1))
    move_fwd.add_effect(sled_sup(s), Minus(sled_sup(s), 1))
    p.add_action(move_fwd)

    # retrieve: value-binding (= (read track ?p) ?v) + write track[p] = v-1
    retrieve = InstantaneousAction('retrieve', s=Sled, pidx=pos_t, v=supply_t)
    s, pidx, v = [retrieve.parameter(x) for x in ['s', 'pidx', 'v']]
    retrieve.add_precondition(Equals(sled_pos(s), pidx))
    retrieve.add_precondition(Equals(track[pidx], v))   # value-bind
    retrieve.add_precondition(GE(v, 1))
    retrieve.add_precondition(GT(sled_cap(s), sled_sup(s)))
    retrieve.add_effect(track[pidx], Minus(v, 1))        # array write
    retrieve.add_effect(sled_sup(s), Plus(sled_sup(s), 1))
    p.add_action(retrieve)

    p.add_goal(Equals(sled_pos(sled), n - 1))
    return p
