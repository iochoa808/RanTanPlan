"""
Cave Diving (Sequential Optimal) — minimal instance.
Features: bounded int (cap), object fluent (diver-at), set fluent (connects),
          forall/when in hire-diver effect.
PDDL-XTS counterpart: xts/benchmarks/translations/cave-diving-sequential-optimal/
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *
from unified_planning.model import Variable


def get_problem():
    p = Problem('minimal-cave-diving-xts')

    Location = UserType('Location')
    Diver    = UserType('Diver')
    Tank     = UserType('Tank')

    # Objects (surface-loc is a domain constant of type location)
    surface_loc = Object('surface-loc', Location)
    l0    = Object('l0', Location)
    l1    = Object('l1', Location)
    d0    = Object('d0', Diver)
    t0    = Object('t0', Tank)
    t1    = Object('t1', Tank)
    t2    = Object('t2', Tank)
    t3    = Object('t3', Tank)
    dummy = Object('dummy', Tank)
    p.add_objects([surface_loc, l0, l1, d0, t0, t1, t2, t3, dummy])

    cap_t  = IntType(0, 3)
    LocSet = SetType(Location)

    # Fluents
    diver_at      = Fluent('diver-at',     Location,  d=Diver)
    connects      = Fluent('connects',     LocSet,    l=Location)
    cap           = Fluent('cap',          cap_t,     d=Diver)
    at_tank       = Fluent('at-tank',      BoolType(), t=Tank, l=Location)
    in_storage    = Fluent('in-storage',   BoolType(), t=Tank)
    full          = Fluent('full',         BoolType(), t=Tank)
    next_tank     = Fluent('next-tank',    BoolType(), t1_=Tank, t2_=Tank)
    available     = Fluent('available',    BoolType(), d=Diver)
    decompressing = Fluent('decompressing', BoolType(), d=Diver)
    precludes     = Fluent('precludes',    BoolType(), d1=Diver, d2=Diver)
    cave_entrance = Fluent('cave-entrance', BoolType(), l=Location)
    holding       = Fluent('holding',      BoolType(), d=Diver, t=Tank)
    have_photo    = Fluent('have-photo',   BoolType(), l=Location)
    in_water      = Fluent('in-water',     BoolType())

    p.add_fluent(diver_at,      default_initial_value=surface_loc)
    p.add_fluent(connects,      default_initial_value=set())
    p.add_fluent(cap,           default_initial_value=Int(0))
    p.add_fluent(at_tank,       default_initial_value=False)
    p.add_fluent(in_storage,    default_initial_value=False)
    p.add_fluent(full,          default_initial_value=False)
    p.add_fluent(next_tank,     default_initial_value=False)
    p.add_fluent(available,     default_initial_value=False)
    p.add_fluent(decompressing, default_initial_value=False)
    p.add_fluent(precludes,     default_initial_value=False)
    p.add_fluent(cave_entrance, default_initial_value=False)
    p.add_fluent(holding,       default_initial_value=False)
    p.add_fluent(have_photo,    default_initial_value=False)
    p.add_fluent(in_water,      default_initial_value=False)

    # Initial state
    p.set_initial_value(available(d0), True)
    p.set_initial_value(cap(d0), Int(3))
    p.set_initial_value(diver_at(d0), surface_loc)
    p.set_initial_value(in_storage(t0), True)
    p.set_initial_value(next_tank(t0, t1), True)
    p.set_initial_value(next_tank(t1, t2), True)
    p.set_initial_value(next_tank(t2, t3), True)
    p.set_initial_value(next_tank(t3, dummy), True)
    p.set_initial_value(cave_entrance(l0), True)
    p.set_initial_value(connects(l0), {l1})
    p.set_initial_value(connects(l1), {l0})
    p.set_initial_value(connects(surface_loc), set())

    # ── Actions ────────────────────────────────────────────────────────────────

    # hire-diver: forall(?d2) when(precludes(d1,d2)) -> not(available(d2))
    hire_diver = InstantaneousAction('hire-diver', d1=Diver)
    d1 = hire_diver.parameter('d1')
    hire_diver.add_precondition(available(d1))
    hire_diver.add_precondition(Not(in_water))
    hire_diver.add_effect(available(d1), False)
    hire_diver.add_effect(in_water, True)
    d2_var = Variable('d2', Diver)
    hire_diver.add_effect(available(d2_var), False,
                          condition=precludes(d1, d2_var), forall=[d2_var])
    p.add_action(hire_diver)

    # prepare-tank
    prepare_tank = InstantaneousAction('prepare-tank', d=Diver, ta=Tank, tb=Tank)
    d, ta, tb = [prepare_tank.parameter(x) for x in ('d', 'ta', 'tb')]
    prepare_tank.add_precondition(Equals(diver_at(d), surface_loc))
    prepare_tank.add_precondition(Not(available(d)))
    prepare_tank.add_precondition(in_storage(ta))
    prepare_tank.add_precondition(next_tank(ta, tb))
    prepare_tank.add_precondition(GE(cap(d), Int(1)))
    prepare_tank.add_effect(in_storage(ta), False)
    prepare_tank.add_effect(in_storage(tb), True)
    prepare_tank.add_effect(full(ta), True)
    prepare_tank.add_effect(holding(d, ta), True)
    prepare_tank.add_effect(cap(d), Minus(cap(d), Int(1)))
    p.add_action(prepare_tank)

    # enter-water
    enter_water = InstantaneousAction('enter-water', d=Diver, l=Location)
    d, l = enter_water.parameter('d'), enter_water.parameter('l')
    enter_water.add_precondition(Equals(diver_at(d), surface_loc))
    enter_water.add_precondition(Not(available(d)))
    enter_water.add_precondition(cave_entrance(l))
    enter_water.add_effect(diver_at(d), l)
    p.add_action(enter_water)

    # pickup-tank
    pickup_tank = InstantaneousAction('pickup-tank', d=Diver, t=Tank, l=Location)
    d, t, l = [pickup_tank.parameter(x) for x in ('d', 't', 'l')]
    pickup_tank.add_precondition(Equals(diver_at(d), l))
    pickup_tank.add_precondition(at_tank(t, l))
    pickup_tank.add_precondition(GE(cap(d), Int(1)))
    pickup_tank.add_effect(at_tank(t, l), False)
    pickup_tank.add_effect(holding(d, t), True)
    pickup_tank.add_effect(cap(d), Minus(cap(d), Int(1)))
    p.add_action(pickup_tank)

    # drop-tank
    drop_tank = InstantaneousAction('drop-tank', d=Diver, t=Tank, l=Location)
    d, t, l = [drop_tank.parameter(x) for x in ('d', 't', 'l')]
    drop_tank.add_precondition(Equals(diver_at(d), l))
    drop_tank.add_precondition(holding(d, t))
    drop_tank.add_precondition(LT(cap(d), Int(3)))
    drop_tank.add_effect(holding(d, t), False)
    drop_tank.add_effect(at_tank(t, l), True)
    drop_tank.add_effect(cap(d), Plus(cap(d), Int(1)))
    p.add_action(drop_tank)

    # swim — use src/dst param names to avoid shadowing the l1/l2 objects
    swim = InstantaneousAction('swim', d=Diver, t=Tank, src=Location, dst=Location)
    d_sw, t_sw, src, dst = [swim.parameter(x) for x in ('d', 't', 'src', 'dst')]
    swim.add_precondition(Equals(diver_at(d_sw), src))
    swim.add_precondition(holding(d_sw, t_sw))
    swim.add_precondition(full(t_sw))
    swim.add_precondition(SetMember(dst, connects(src)))
    swim.add_effect(diver_at(d_sw), dst)
    swim.add_effect(full(t_sw), False)
    p.add_action(swim)

    # photograph
    photograph = InstantaneousAction('photograph', d=Diver, loc=Location, t=Tank)
    d_ph, loc_ph, t_ph = [photograph.parameter(x) for x in ('d', 'loc', 't')]
    photograph.add_precondition(Equals(diver_at(d_ph), loc_ph))
    photograph.add_precondition(holding(d_ph, t_ph))
    photograph.add_precondition(full(t_ph))
    photograph.add_effect(full(t_ph), False)
    photograph.add_effect(have_photo(loc_ph), True)
    p.add_action(photograph)

    # decompress
    decompress = InstantaneousAction('decompress', d=Diver, loc=Location)
    d_dc, loc_dc = decompress.parameter('d'), decompress.parameter('loc')
    decompress.add_precondition(Equals(diver_at(d_dc), loc_dc))
    decompress.add_precondition(cave_entrance(loc_dc))
    decompress.add_effect(diver_at(d_dc), surface_loc)
    decompress.add_effect(decompressing(d_dc), True)
    decompress.add_effect(in_water, False)
    p.add_action(decompress)

    # Goal — use the original object references (not rebound by action params)
    p.add_goal(have_photo(l1))
    p.add_goal(decompressing(d0))

    return p
