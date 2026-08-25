"""
Rover — p14: 4 rovers, 10 waypoints, 5 cameras, 4 objectives.

Features: SetType(Waypoint) for soil/rock analysis and communication sets,
bounded energy IntType(0,100), many binary predicates.
Likely exceeds 30s timeout; included for completeness.
PDDL-XTS counterpart: PDDL-XTS/rover/instances/p14.pddl
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('rover_p14')

    Rover    = UserType('Rover')
    Waypoint = UserType('Waypoint')
    Store    = UserType('Store')
    Camera   = UserType('Camera')
    Mode     = UserType('Mode')
    Lander   = UserType('Lander')
    Objective = UserType('Objective')

    general = Object('general', Lander)
    colour  = Object('colour',  Mode)
    high_res = Object('high_res', Mode)
    low_res  = Object('low_res',  Mode)
    p.add_objects([general, colour, high_res, low_res])

    rovers = [Object(f'rover{i}', Rover) for i in range(4)]
    stores = [Object(f'rover{i}store', Store) for i in range(4)]
    wps    = [Object(f'waypoint{i}', Waypoint) for i in range(10)]
    cams   = [Object(f'camera{i}', Camera) for i in range(5)]
    objs   = [Object(f'objective{i}', Objective) for i in range(4)]
    p.add_objects(rovers + stores + wps + cams + objs)

    r0, r1, r2, r3 = rovers
    s0, s1, s2, s3 = stores
    w = {i: wps[i] for i in range(10)}
    c0, c1, c2, c3, c4 = cams
    o0, o1, o2, o3 = objs

    energy_t = IntType(0, 100)

    in_wp      = Fluent('in_wp',      BoolType(), r=Rover, wp=Waypoint)
    at_lander  = Fluent('at_lander',  BoolType(), l=Lander, wp=Waypoint)
    can_trav   = Fluent('can_traverse', BoolType(), r=Rover, x=Waypoint, y=Waypoint)
    eq_soil    = Fluent('equipped_for_soil_analysis', BoolType(), r=Rover)
    eq_rock    = Fluent('equipped_for_rock_analysis', BoolType(), r=Rover)
    eq_img     = Fluent('equipped_for_imaging',       BoolType(), r=Rover)
    empty_st   = Fluent('empty', BoolType(), s=Store)
    full_st    = Fluent('full',  BoolType(), s=Store)
    calibrated = Fluent('calibrated', BoolType(), c=Camera, r=Rover)
    supports   = Fluent('supports',   BoolType(), c=Camera, m=Mode)
    available  = Fluent('available',  BoolType(), r=Rover)
    visible    = Fluent('visible',    BoolType(), x=Waypoint, y=Waypoint)
    have_image = Fluent('have_image', BoolType(), r=Rover, o=Objective, m=Mode)
    comm_img   = Fluent('communicated_image_data', BoolType(), o=Objective, m=Mode)
    at_soil    = Fluent('at_soil_sample', BoolType(), wp=Waypoint)
    at_rock    = Fluent('at_rock_sample', BoolType(), wp=Waypoint)
    vis_from   = Fluent('visible_from',   BoolType(), o=Objective, wp=Waypoint)
    store_of   = Fluent('store_of',       BoolType(), s=Store, r=Rover)
    cal_target = Fluent('calibration_target', BoolType(), cam=Camera, o=Objective)
    on_board   = Fluent('on_board',       BoolType(), cam=Camera, r=Rover)
    chan_free   = Fluent('channel_free',  BoolType(), l=Lander)
    in_sun     = Fluent('in_sun',        BoolType(), wp=Waypoint)

    energy          = Fluent('energy',          energy_t, r=Rover)
    recharges       = Fluent('recharges',        IntType(0, 100))
    soil_analyzed   = Fluent('soil_analyzed',    SetType(Waypoint), r=Rover)
    rock_analyzed   = Fluent('rock_analyzed',    SetType(Waypoint), r=Rover)
    comm_soil       = Fluent('communicated_soil', SetType(Waypoint))
    comm_rock       = Fluent('communicated_rock', SetType(Waypoint))

    for f in [in_wp, at_lander, can_trav, eq_soil, eq_rock, eq_img,
              empty_st, full_st, calibrated, supports, available, visible,
              have_image, comm_img, at_soil, at_rock, vis_from, store_of,
              cal_target, on_board, chan_free, in_sun]:
        p.add_fluent(f, default_initial_value=False)
    p.add_fluent(energy,        default_initial_value=50)
    p.add_fluent(recharges,     default_initial_value=0)
    p.add_fluent(soil_analyzed, default_initial_value=set())
    p.add_fluent(rock_analyzed, default_initial_value=set())
    p.add_fluent(comm_soil,     default_initial_value=set())
    p.add_fluent(comm_rock,     default_initial_value=set())

    # explicit inits required for object fluents (no default propagation)
    for r in rovers:
        p.set_initial_value(soil_analyzed(r), set())
        p.set_initial_value(rock_analyzed(r), set())
    p.set_initial_value(comm_soil, set())
    p.set_initial_value(comm_rock, set())

    # ── Initial state ──────────────────────────────────────────────────────────

    visible_pairs = [
        (0,3),(3,0),(0,9),(9,0),(1,8),(8,1),(2,1),(1,2),(2,3),(3,2),(2,6),(6,2),
        (2,9),(9,2),(3,5),(5,3),(3,6),(6,3),(3,7),(7,3),(4,0),(0,4),(4,1),(1,4),
        (4,2),(2,4),(4,8),(8,4),(4,9),(9,4),(5,1),(1,5),(5,2),(2,5),(5,4),(4,5),
        (5,6),(6,5),(6,0),(0,6),(6,1),(1,6),(6,4),(4,6),(7,1),(1,7),(7,5),(5,7),
        (7,8),(8,7),(8,6),(6,8),(8,9),(9,8),(9,3),(3,9),(9,6),(6,9),
    ]
    for a, b in visible_pairs:
        p.set_initial_value(visible(w[a], w[b]), True)

    for wp in [w[1], w[3], w[4], w[5], w[8]]:
        p.set_initial_value(at_rock(wp), True)
    for wp in [w[3], w[4], w[6], w[9]]:
        p.set_initial_value(at_soil(wp), True)
    for wp in [w[5], w[9]]:
        p.set_initial_value(in_sun(wp), True)

    p.set_initial_value(at_lander(general, w[7]), True)
    p.set_initial_value(chan_free(general), True)
    p.set_initial_value(recharges, 0)

    # rover0
    p.set_initial_value(energy(r0), 50)
    p.set_initial_value(in_wp(r0, w[1]), True)
    p.set_initial_value(available(r0), True)
    p.set_initial_value(store_of(s0, r0), True)
    p.set_initial_value(empty_st(s0), True)
    p.set_initial_value(eq_soil(r0), True)
    p.set_initial_value(eq_img(r0), True)
    for a, b in [(1,2),(2,1),(1,4),(4,1),(1,6),(6,1),(1,8),(8,1),
                 (2,3),(3,2),(4,0),(0,4),(4,5),(5,4),(4,9),(9,4),(8,7),(7,8)]:
        p.set_initial_value(can_trav(r0, w[a], w[b]), True)

    # rover1
    p.set_initial_value(energy(r1), 50)
    p.set_initial_value(in_wp(r1, w[4]), True)
    p.set_initial_value(available(r1), True)
    p.set_initial_value(store_of(s1, r1), True)
    p.set_initial_value(empty_st(s1), True)
    p.set_initial_value(eq_soil(r1), True)
    p.set_initial_value(eq_rock(r1), True)
    p.set_initial_value(eq_img(r1), True)
    for a, b in [(4,0),(0,4),(4,2),(2,4),(4,5),(5,4),(4,6),(6,4),(4,9),(9,4),
                 (0,3),(3,0),(2,1),(1,2),(5,7),(7,5),(6,8),(8,6)]:
        p.set_initial_value(can_trav(r1, w[a], w[b]), True)

    # rover2
    p.set_initial_value(energy(r2), 50)
    p.set_initial_value(in_wp(r2, w[0]), True)
    p.set_initial_value(available(r2), True)
    p.set_initial_value(store_of(s2, r2), True)
    p.set_initial_value(empty_st(s2), True)
    p.set_initial_value(eq_img(r2), True)
    for a, b in [(0,3),(3,0),(0,4),(4,0),(0,6),(6,0),(0,9),(9,0),
                 (3,2),(2,3),(4,1),(1,4),(4,5),(5,4),(4,8),(8,4)]:
        p.set_initial_value(can_trav(r2, w[a], w[b]), True)

    # rover3
    p.set_initial_value(energy(r3), 50)
    p.set_initial_value(in_wp(r3, w[2]), True)
    p.set_initial_value(available(r3), True)
    p.set_initial_value(store_of(s3, r3), True)
    p.set_initial_value(empty_st(s3), True)
    p.set_initial_value(eq_img(r3), True)
    for a, b in [(2,1),(1,2),(2,6),(6,2),(2,9),(9,2),
                 (1,4),(4,1),(1,5),(5,1),(1,7),(7,1),
                 (6,0),(0,6),(6,3),(3,6),(6,8),(8,6)]:
        p.set_initial_value(can_trav(r3, w[a], w[b]), True)

    # cameras
    p.set_initial_value(on_board(c0, r3), True)
    p.set_initial_value(cal_target(c0, o2), True)
    p.set_initial_value(supports(c0, colour), True)
    p.set_initial_value(supports(c0, low_res), True)

    p.set_initial_value(on_board(c1, r2), True)
    p.set_initial_value(cal_target(c1, o3), True)
    p.set_initial_value(supports(c1, colour), True)

    p.set_initial_value(on_board(c2, r1), True)
    p.set_initial_value(cal_target(c2, o3), True)
    p.set_initial_value(supports(c2, low_res), True)

    p.set_initial_value(on_board(c3, r1), True)
    p.set_initial_value(cal_target(c3, o0), True)
    p.set_initial_value(supports(c3, colour), True)
    p.set_initial_value(supports(c3, low_res), True)

    p.set_initial_value(on_board(c4, r0), True)
    p.set_initial_value(cal_target(c4, o3), True)
    p.set_initial_value(supports(c4, colour), True)
    p.set_initial_value(supports(c4, low_res), True)

    for oi, wps_list in [
        (o0, [0,1,2,3,4,5,6]),
        (o1, [0,1,2,3,4,5,6]),
        (o2, [0,1,2,3,4,5,6,7,8]),
        (o3, [0,1,2,3,4,5,6]),
    ]:
        for wi in wps_list:
            p.set_initial_value(vis_from(oi, w[wi]), True)

    # ── Actions ────────────────────────────────────────────────────────────────

    navigate = InstantaneousAction('navigate', r=Rover, frm=Waypoint, to=Waypoint)
    r, frm, to = [navigate.parameter(x) for x in ('r', 'frm', 'to')]
    navigate.add_precondition(can_trav(r, frm, to))
    navigate.add_precondition(available(r))
    navigate.add_precondition(in_wp(r, frm))
    navigate.add_precondition(visible(frm, to))
    navigate.add_precondition(GE(energy(r), 8))
    navigate.add_effect(energy(r), Minus(energy(r), 8))
    navigate.add_effect(in_wp(r, frm), False)
    navigate.add_effect(in_wp(r, to), True)
    p.add_action(navigate)

    recharge_act = InstantaneousAction('recharge', r=Rover, wp=Waypoint)
    r, wp = [recharge_act.parameter(x) for x in ('r', 'wp')]
    recharge_act.add_precondition(in_wp(r, wp))
    recharge_act.add_precondition(in_sun(wp))
    recharge_act.add_precondition(LE(energy(r), 80))
    recharge_act.add_effect(energy(r), Plus(energy(r), 20))
    recharge_act.add_effect(recharges, Plus(recharges, 1))
    p.add_action(recharge_act)

    sample_soil = InstantaneousAction('sample_soil', r=Rover, s=Store, wp=Waypoint)
    r, s, wp = [sample_soil.parameter(x) for x in ('r', 's', 'wp')]
    sample_soil.add_precondition(in_wp(r, wp))
    sample_soil.add_precondition(GE(energy(r), 3))
    sample_soil.add_precondition(at_soil(wp))
    sample_soil.add_precondition(eq_soil(r))
    sample_soil.add_precondition(store_of(s, r))
    sample_soil.add_precondition(empty_st(s))
    sample_soil.add_effect(empty_st(s), False)
    sample_soil.add_effect(full_st(s), True)
    sample_soil.add_effect(energy(r), Minus(energy(r), 3))
    sample_soil.add_effect(soil_analyzed(r), SetAdd(wp, soil_analyzed(r)))
    sample_soil.add_effect(at_soil(wp), False)
    p.add_action(sample_soil)

    sample_rock = InstantaneousAction('sample_rock', r=Rover, s=Store, wp=Waypoint)
    r, s, wp = [sample_rock.parameter(x) for x in ('r', 's', 'wp')]
    sample_rock.add_precondition(in_wp(r, wp))
    sample_rock.add_precondition(GE(energy(r), 5))
    sample_rock.add_precondition(at_rock(wp))
    sample_rock.add_precondition(eq_rock(r))
    sample_rock.add_precondition(store_of(s, r))
    sample_rock.add_precondition(empty_st(s))
    sample_rock.add_effect(empty_st(s), False)
    sample_rock.add_effect(full_st(s), True)
    sample_rock.add_effect(energy(r), Minus(energy(r), 5))
    sample_rock.add_effect(rock_analyzed(r), SetAdd(wp, rock_analyzed(r)))
    sample_rock.add_effect(at_rock(wp), False)
    p.add_action(sample_rock)

    drop = InstantaneousAction('drop', r=Rover, s=Store)
    r, s = [drop.parameter(x) for x in ('r', 's')]
    drop.add_precondition(store_of(s, r))
    drop.add_precondition(full_st(s))
    drop.add_effect(full_st(s), False)
    drop.add_effect(empty_st(s), True)
    p.add_action(drop)

    calibrate = InstantaneousAction('calibrate', r=Rover, cam=Camera, obj=Objective, wp=Waypoint)
    r, cam, obj, wp = [calibrate.parameter(x) for x in ('r', 'cam', 'obj', 'wp')]
    calibrate.add_precondition(eq_img(r))
    calibrate.add_precondition(GE(energy(r), 2))
    calibrate.add_precondition(cal_target(cam, obj))
    calibrate.add_precondition(in_wp(r, wp))
    calibrate.add_precondition(vis_from(obj, wp))
    calibrate.add_precondition(on_board(cam, r))
    calibrate.add_effect(energy(r), Minus(energy(r), 2))
    calibrate.add_effect(calibrated(cam, r), True)
    p.add_action(calibrate)

    take_image = InstantaneousAction('take_image', r=Rover, wp=Waypoint, obj=Objective, cam=Camera, m=Mode)
    r, wp, obj, cam, m = [take_image.parameter(x) for x in ('r', 'wp', 'obj', 'cam', 'm')]
    take_image.add_precondition(calibrated(cam, r))
    take_image.add_precondition(on_board(cam, r))
    take_image.add_precondition(eq_img(r))
    take_image.add_precondition(supports(cam, m))
    take_image.add_precondition(vis_from(obj, wp))
    take_image.add_precondition(in_wp(r, wp))
    take_image.add_precondition(GE(energy(r), 1))
    take_image.add_effect(have_image(r, obj, m), True)
    take_image.add_effect(calibrated(cam, r), False)
    take_image.add_effect(energy(r), Minus(energy(r), 1))
    p.add_action(take_image)

    comm_soil_act = InstantaneousAction('communicate_soil_data',
                                        r=Rover, l=Lander, wp=Waypoint, x=Waypoint, y=Waypoint)
    r, l, wp, x, y = [comm_soil_act.parameter(n) for n in ('r', 'l', 'wp', 'x', 'y')]
    comm_soil_act.add_precondition(in_wp(r, x))
    comm_soil_act.add_precondition(at_lander(l, y))
    comm_soil_act.add_precondition(SetMember(wp, soil_analyzed(r)))
    comm_soil_act.add_precondition(visible(x, y))
    comm_soil_act.add_precondition(available(r))
    comm_soil_act.add_precondition(chan_free(l))
    comm_soil_act.add_precondition(GE(energy(r), 4))
    comm_soil_act.add_effect(comm_soil, SetAdd(wp, comm_soil))
    comm_soil_act.add_effect(energy(r), Minus(energy(r), 4))
    p.add_action(comm_soil_act)

    comm_rock_act = InstantaneousAction('communicate_rock_data',
                                        r=Rover, l=Lander, wp=Waypoint, x=Waypoint, y=Waypoint)
    r, l, wp, x, y = [comm_rock_act.parameter(n) for n in ('r', 'l', 'wp', 'x', 'y')]
    comm_rock_act.add_precondition(in_wp(r, x))
    comm_rock_act.add_precondition(at_lander(l, y))
    comm_rock_act.add_precondition(SetMember(wp, rock_analyzed(r)))
    comm_rock_act.add_precondition(visible(x, y))
    comm_rock_act.add_precondition(available(r))
    comm_rock_act.add_precondition(chan_free(l))
    comm_rock_act.add_precondition(GE(energy(r), 4))
    comm_rock_act.add_effect(comm_rock, SetAdd(wp, comm_rock))
    comm_rock_act.add_effect(energy(r), Minus(energy(r), 4))
    p.add_action(comm_rock_act)

    comm_img_act = InstantaneousAction('communicate_image_data',
                                       r=Rover, l=Lander, obj=Objective, m=Mode, x=Waypoint, y=Waypoint)
    r, l, obj, m, x, y = [comm_img_act.parameter(n) for n in ('r', 'l', 'obj', 'm', 'x', 'y')]
    comm_img_act.add_precondition(in_wp(r, x))
    comm_img_act.add_precondition(at_lander(l, y))
    comm_img_act.add_precondition(have_image(r, obj, m))
    comm_img_act.add_precondition(visible(x, y))
    comm_img_act.add_precondition(available(r))
    comm_img_act.add_precondition(chan_free(l))
    comm_img_act.add_precondition(GE(energy(r), 6))
    comm_img_act.add_effect(comm_img(obj, m), True)
    comm_img_act.add_effect(energy(r), Minus(energy(r), 6))
    p.add_action(comm_img_act)

    # ── Goal ──────────────────────────────────────────────────────────────────
    p.add_goal(SetMember(w[3], comm_soil))
    p.add_goal(SetMember(w[6], comm_soil))
    p.add_goal(SetMember(w[5], comm_rock))
    p.add_goal(SetMember(w[4], comm_rock))
    p.add_goal(SetMember(w[8], comm_rock))
    p.add_goal(comm_img(o0, colour))
    p.add_goal(comm_img(o2, low_res))
    p.add_goal(comm_img(o0, low_res))

    return p
