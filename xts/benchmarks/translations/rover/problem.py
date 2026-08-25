"""
Rover XTS — roverprob1234.
Features: bounded int (energy), set fluent (visible-to), object fluents.
Plan: ~10 steps.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('roverprob1234-xts')

    Rover     = UserType('rover')
    Waypoint  = UserType('waypoint')
    Store     = UserType('store')
    Camera    = UserType('camera')
    Mode      = UserType('mode')
    Lander    = UserType('lander')
    Objective = UserType('objective')

    nrg_t    = IntType(0, 80)
    rch_t    = IntType(0, 10)
    WPSet    = SetType(Waypoint)

    # Objects
    general   = Object('general',   Lander)
    colour    = Object('colour',    Mode)
    high_res  = Object('high_res',  Mode)
    low_res   = Object('low_res',   Mode)
    rover0    = Object('rover0',    Rover)
    rover0store = Object('rover0store', Store)
    wp0 = Object('waypoint0', Waypoint)
    wp1 = Object('waypoint1', Waypoint)
    wp2 = Object('waypoint2', Waypoint)
    wp3 = Object('waypoint3', Waypoint)
    camera0   = Object('camera0',   Camera)
    obj0      = Object('objective0', Objective)
    obj1      = Object('objective1', Objective)

    p.add_objects([general, colour, high_res, low_res,
                   rover0, rover0store, wp0, wp1, wp2, wp3,
                   camera0, obj0, obj1])

    # Fluents
    location_f     = Fluent('location',        Waypoint,  r=Rover)
    lander_location= Fluent('lander_location', Waypoint,  l=Lander)
    store_rover    = Fluent('store_rover',      Rover,     s=Store)
    camera_rover   = Fluent('camera_rover',     Rover,     c=Camera)
    cal_target     = Fluent('cal_target',       Objective, c=Camera)
    energy         = Fluent('energy',           nrg_t,     r=Rover)
    recharges      = Fluent('recharges',        rch_t)
    visible_to     = Fluent('visible-to',       WPSet,     w=Waypoint)

    can_traverse    = Fluent('can_traverse',    r=Rover, x=Waypoint, y=Waypoint)
    equipped_soil   = Fluent('equipped_for_soil_analysis', r=Rover)
    equipped_rock   = Fluent('equipped_for_rock_analysis', r=Rover)
    equipped_imaging= Fluent('equipped_for_imaging',       r=Rover)
    empty_s         = Fluent('empty',           s=Store)
    have_rock       = Fluent('have_rock_analysis',  r=Rover, w=Waypoint)
    have_soil       = Fluent('have_soil_analysis',  r=Rover, w=Waypoint)
    full_s          = Fluent('full',            s=Store)
    calibrated      = Fluent('calibrated',      c=Camera)
    supports        = Fluent('supports',        c=Camera, m=Mode)
    available_r     = Fluent('available',       r=Rover)
    have_image      = Fluent('have_image',      r=Rover, o=Objective, m=Mode)
    comm_soil       = Fluent('communicated_soil_data',  w=Waypoint)
    comm_rock       = Fluent('communicated_rock_data',  w=Waypoint)
    comm_image      = Fluent('communicated_image_data', o=Objective, m=Mode)
    at_soil_sample  = Fluent('at_soil_sample',  w=Waypoint)
    at_rock_sample  = Fluent('at_rock_sample',  w=Waypoint)
    visible_from    = Fluent('visible_from',    o=Objective, w=Waypoint)
    channel_free    = Fluent('channel_free',    l=Lander)
    in_sun          = Fluent('in_sun',          w=Waypoint)

    p.add_fluent(location_f,      default_initial_value=wp0)
    p.add_fluent(lander_location, default_initial_value=wp0)
    p.add_fluent(store_rover,     default_initial_value=rover0)
    p.add_fluent(camera_rover,    default_initial_value=rover0)
    p.add_fluent(cal_target,      default_initial_value=obj0)
    p.add_fluent(energy,          default_initial_value=Int(0))
    p.add_fluent(recharges,       default_initial_value=Int(0))
    p.add_fluent(visible_to,      default_initial_value=set())
    p.add_fluent(can_traverse,    default_initial_value=False)
    p.add_fluent(equipped_soil,   default_initial_value=False)
    p.add_fluent(equipped_rock,   default_initial_value=False)
    p.add_fluent(equipped_imaging,default_initial_value=False)
    p.add_fluent(empty_s,         default_initial_value=False)
    p.add_fluent(have_rock,       default_initial_value=False)
    p.add_fluent(have_soil,       default_initial_value=False)
    p.add_fluent(full_s,          default_initial_value=False)
    p.add_fluent(calibrated,      default_initial_value=False)
    p.add_fluent(supports,        default_initial_value=False)
    p.add_fluent(available_r,     default_initial_value=False)
    p.add_fluent(have_image,      default_initial_value=False)
    p.add_fluent(comm_soil,       default_initial_value=False)
    p.add_fluent(comm_rock,       default_initial_value=False)
    p.add_fluent(comm_image,      default_initial_value=False)
    p.add_fluent(at_soil_sample,  default_initial_value=False)
    p.add_fluent(at_rock_sample,  default_initial_value=False)
    p.add_fluent(visible_from,    default_initial_value=False)
    p.add_fluent(channel_free,    default_initial_value=False)
    p.add_fluent(in_sun,          default_initial_value=False)

    # Init
    p.set_initial_value(location_f(rover0),      wp3)
    p.set_initial_value(energy(rover0),          Int(50))
    p.set_initial_value(recharges,               Int(0))
    p.set_initial_value(available_r(rover0),     True)

    p.set_initial_value(lander_location(general), wp0)
    p.set_initial_value(store_rover(rover0store), rover0)
    p.set_initial_value(camera_rover(camera0),    rover0)
    p.set_initial_value(cal_target(camera0),      obj1)

    p.set_initial_value(equipped_soil(rover0),    True)
    p.set_initial_value(equipped_rock(rover0),    True)
    p.set_initial_value(equipped_imaging(rover0), True)
    p.set_initial_value(empty_s(rover0store),     True)

    p.set_initial_value(can_traverse(rover0, wp3, wp0), True)
    p.set_initial_value(can_traverse(rover0, wp0, wp3), True)
    p.set_initial_value(can_traverse(rover0, wp3, wp1), True)
    p.set_initial_value(can_traverse(rover0, wp1, wp3), True)
    p.set_initial_value(can_traverse(rover0, wp1, wp2), True)
    p.set_initial_value(can_traverse(rover0, wp2, wp1), True)

    p.set_initial_value(visible_to(wp0), {wp1, wp2, wp3})
    p.set_initial_value(visible_to(wp1), {wp0, wp2, wp3})
    p.set_initial_value(visible_to(wp2), {wp0, wp1, wp3})
    p.set_initial_value(visible_to(wp3), {wp0, wp1, wp2})

    p.set_initial_value(supports(camera0, colour),   True)
    p.set_initial_value(supports(camera0, high_res), True)

    for wp in [wp0, wp1, wp2, wp3]:
        p.set_initial_value(visible_from(obj0, wp), True)
        p.set_initial_value(visible_from(obj1, wp), True)

    p.set_initial_value(channel_free(general), True)

    p.set_initial_value(at_soil_sample(wp0), True)
    p.set_initial_value(in_sun(wp0),         True)
    p.set_initial_value(at_rock_sample(wp1), True)
    p.set_initial_value(at_soil_sample(wp2), True)
    p.set_initial_value(at_rock_sample(wp2), True)
    p.set_initial_value(at_soil_sample(wp3), True)
    p.set_initial_value(at_rock_sample(wp3), True)

    # ---- Actions ----

    navigate = InstantaneousAction('navigate', x=Rover, y=Waypoint, z=Waypoint)
    x_p, y_p, z_p = [navigate.parameter(n) for n in ('x', 'y', 'z')]
    navigate.add_precondition(can_traverse(x_p, y_p, z_p))
    navigate.add_precondition(available_r(x_p))
    navigate.add_precondition(Equals(location_f(x_p), y_p))
    navigate.add_precondition(SetMember(z_p, visible_to(y_p)))
    navigate.add_precondition(GE(energy(x_p), Int(8)))
    navigate.add_effect(energy(x_p),    Minus(energy(x_p), Int(8)))
    navigate.add_effect(location_f(x_p), z_p)
    p.add_action(navigate)

    recharge = InstantaneousAction('recharge', x=Rover, w=Waypoint)
    x_p, w_p = recharge.parameter('x'), recharge.parameter('w')
    recharge.add_precondition(Equals(location_f(x_p), w_p))
    recharge.add_precondition(in_sun(w_p))
    recharge.add_effect(energy(x_p),  Plus(energy(x_p), Int(20)))
    recharge.add_effect(recharges,    Plus(recharges, Int(1)))
    p.add_action(recharge)

    sample_soil = InstantaneousAction('sample_soil', x=Rover, s=Store, wp=Waypoint)
    x_p, s_p, wp_p = [sample_soil.parameter(n) for n in ('x', 's', 'wp')]
    sample_soil.add_precondition(Equals(location_f(x_p), wp_p))
    sample_soil.add_precondition(GE(energy(x_p), Int(3)))
    sample_soil.add_precondition(at_soil_sample(wp_p))
    sample_soil.add_precondition(equipped_soil(x_p))
    sample_soil.add_precondition(Equals(store_rover(s_p), x_p))
    sample_soil.add_precondition(empty_s(s_p))
    sample_soil.add_effect(empty_s(s_p), False)
    sample_soil.add_effect(full_s(s_p),  True)
    sample_soil.add_effect(energy(x_p),  Minus(energy(x_p), Int(3)))
    sample_soil.add_effect(have_soil(x_p, wp_p), True)
    sample_soil.add_effect(at_soil_sample(wp_p), False)
    p.add_action(sample_soil)

    sample_rock = InstantaneousAction('sample_rock', x=Rover, s=Store, wp=Waypoint)
    x_p, s_p, wp_p = [sample_rock.parameter(n) for n in ('x', 's', 'wp')]
    sample_rock.add_precondition(Equals(location_f(x_p), wp_p))
    sample_rock.add_precondition(GE(energy(x_p), Int(5)))
    sample_rock.add_precondition(at_rock_sample(wp_p))
    sample_rock.add_precondition(equipped_rock(x_p))
    sample_rock.add_precondition(Equals(store_rover(s_p), x_p))
    sample_rock.add_precondition(empty_s(s_p))
    sample_rock.add_effect(empty_s(s_p), False)
    sample_rock.add_effect(full_s(s_p),  True)
    sample_rock.add_effect(energy(x_p),  Minus(energy(x_p), Int(5)))
    sample_rock.add_effect(have_rock(x_p, wp_p), True)
    sample_rock.add_effect(at_rock_sample(wp_p), False)
    p.add_action(sample_rock)

    drop = InstantaneousAction('drop', x=Rover, y=Store)
    x_p, y_p = drop.parameter('x'), drop.parameter('y')
    drop.add_precondition(Equals(store_rover(y_p), x_p))
    drop.add_precondition(full_s(y_p))
    drop.add_effect(full_s(y_p),  False)
    drop.add_effect(empty_s(y_p), True)
    p.add_action(drop)

    calibrate = InstantaneousAction('calibrate', r=Rover, i=Camera, t=Objective, w=Waypoint)
    r_p, i_p, t_p, w_p = [calibrate.parameter(n) for n in ('r', 'i', 't', 'w')]
    calibrate.add_precondition(equipped_imaging(r_p))
    calibrate.add_precondition(GE(energy(r_p), Int(2)))
    calibrate.add_precondition(Equals(cal_target(i_p), t_p))
    calibrate.add_precondition(Equals(location_f(r_p), w_p))
    calibrate.add_precondition(visible_from(t_p, w_p))
    calibrate.add_precondition(Equals(camera_rover(i_p), r_p))
    calibrate.add_effect(energy(r_p),  Minus(energy(r_p), Int(2)))
    calibrate.add_effect(calibrated(i_p), True)
    p.add_action(calibrate)

    take_image = InstantaneousAction('take_image', r=Rover, wp=Waypoint, o=Objective, i=Camera, m=Mode)
    r_p, wp_p, o_p, i_p, m_p = [take_image.parameter(n) for n in ('r', 'wp', 'o', 'i', 'm')]
    take_image.add_precondition(calibrated(i_p))
    take_image.add_precondition(Equals(camera_rover(i_p), r_p))
    take_image.add_precondition(equipped_imaging(r_p))
    take_image.add_precondition(supports(i_p, m_p))
    take_image.add_precondition(visible_from(o_p, wp_p))
    take_image.add_precondition(Equals(location_f(r_p), wp_p))
    take_image.add_precondition(GE(energy(r_p), Int(1)))
    take_image.add_effect(have_image(r_p, o_p, m_p), True)
    take_image.add_effect(calibrated(i_p), False)
    take_image.add_effect(energy(r_p), Minus(energy(r_p), Int(1)))
    p.add_action(take_image)

    comm_soil_act = InstantaneousAction('communicate_soil_data',
                                        r=Rover, l=Lander, wp=Waypoint, x=Waypoint, y=Waypoint)
    r_p, l_p, wp_p, x_p, y_p = [comm_soil_act.parameter(n) for n in ('r', 'l', 'wp', 'x', 'y')]
    comm_soil_act.add_precondition(Equals(location_f(r_p), x_p))
    comm_soil_act.add_precondition(Equals(lander_location(l_p), y_p))
    comm_soil_act.add_precondition(have_soil(r_p, wp_p))
    comm_soil_act.add_precondition(SetMember(y_p, visible_to(x_p)))
    comm_soil_act.add_precondition(available_r(r_p))
    comm_soil_act.add_precondition(channel_free(l_p))
    comm_soil_act.add_precondition(GE(energy(r_p), Int(4)))
    comm_soil_act.add_effect(comm_soil(wp_p),  True)
    comm_soil_act.add_effect(available_r(r_p), True)
    comm_soil_act.add_effect(energy(r_p),      Minus(energy(r_p), Int(4)))
    p.add_action(comm_soil_act)

    comm_rock_act = InstantaneousAction('communicate_rock_data',
                                        r=Rover, l=Lander, wp=Waypoint, x=Waypoint, y=Waypoint)
    r_p, l_p, wp_p, x_p, y_p = [comm_rock_act.parameter(n) for n in ('r', 'l', 'wp', 'x', 'y')]
    comm_rock_act.add_precondition(Equals(location_f(r_p), x_p))
    comm_rock_act.add_precondition(Equals(lander_location(l_p), y_p))
    comm_rock_act.add_precondition(have_rock(r_p, wp_p))
    comm_rock_act.add_precondition(SetMember(y_p, visible_to(x_p)))
    comm_rock_act.add_precondition(available_r(r_p))
    comm_rock_act.add_precondition(channel_free(l_p))
    comm_rock_act.add_precondition(GE(energy(r_p), Int(4)))
    comm_rock_act.add_effect(comm_rock(wp_p),  True)
    comm_rock_act.add_effect(available_r(r_p), True)
    comm_rock_act.add_effect(energy(r_p),      Minus(energy(r_p), Int(4)))
    p.add_action(comm_rock_act)

    comm_img_act = InstantaneousAction('communicate_image_data',
                                       r=Rover, l=Lander, o=Objective, m=Mode, x=Waypoint, y=Waypoint)
    r_p, l_p, o_p, m_p, x_p, y_p = [comm_img_act.parameter(n) for n in ('r', 'l', 'o', 'm', 'x', 'y')]
    comm_img_act.add_precondition(Equals(location_f(r_p), x_p))
    comm_img_act.add_precondition(Equals(lander_location(l_p), y_p))
    comm_img_act.add_precondition(have_image(r_p, o_p, m_p))
    comm_img_act.add_precondition(SetMember(y_p, visible_to(x_p)))
    comm_img_act.add_precondition(available_r(r_p))
    comm_img_act.add_precondition(channel_free(l_p))
    comm_img_act.add_precondition(GE(energy(r_p), Int(6)))
    comm_img_act.add_effect(comm_image(o_p, m_p), True)
    comm_img_act.add_effect(available_r(r_p),      True)
    comm_img_act.add_effect(energy(r_p),           Minus(energy(r_p), Int(6)))
    p.add_action(comm_img_act)

    # Goals
    p.add_goal(comm_soil(wp2))
    p.add_goal(comm_rock(wp3))
    p.add_goal(comm_image(obj1, high_res))

    return p
