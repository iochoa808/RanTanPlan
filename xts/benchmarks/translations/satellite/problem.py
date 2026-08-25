"""
Satellite XTS — image two targets with one instrument.
XTS features: sets (supports), object fluents (pointing-at, on-board, cal-target).
Plan: 6 steps.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('simple_satellite_xts')

    Satellite  = UserType('satellite')
    Direction  = UserType('direction')
    Instrument = UserType('instrument')
    Mode       = UserType('mode')

    # Boolean predicates
    power_avail = Fluent('power_avail', s=Satellite)
    power_on    = Fluent('power_on', i=Instrument)
    calibrated  = Fluent('calibrated', i=Instrument)
    have_image  = Fluent('have_image', d=Direction, m=Mode)
    for f in [power_avail, power_on, calibrated, have_image]:
        p.add_fluent(f, default_initial_value=False)

    # Set fluent: (supports ?i) - modeset
    supports = Fluent('supports', SetType(Mode), i=Instrument)
    p.add_fluent(supports, default_initial_value=set())

    # Object fluents
    pointing_at = Fluent('pointing_at', Direction, s=Satellite)
    on_board    = Fluent('on_board',    Satellite,  i=Instrument)
    cal_target  = Fluent('cal_target',  Direction,  i=Instrument)
    for f in [pointing_at, on_board, cal_target]:
        p.add_fluent(f)

    # Objects
    satellite0   = Object('satellite0',   Satellite)
    instrument0  = Object('instrument0',  Instrument)
    thermograph0 = Object('thermograph0', Mode)
    GroundStation = Object('GroundStation', Direction)
    Target1       = Object('Target1',       Direction)
    Target2       = Object('Target2',       Direction)
    p.add_objects([satellite0, instrument0, thermograph0,
                   GroundStation, Target1, Target2])

    # Initial state
    p.set_initial_value(supports(instrument0), {thermograph0})
    p.set_initial_value(cal_target(instrument0), GroundStation)
    p.set_initial_value(on_board(instrument0), satellite0)
    p.set_initial_value(power_avail(satellite0), True)
    p.set_initial_value(pointing_at(satellite0), GroundStation)

    # Action: turn_to(?s, ?d_new, ?d_prev)
    turn_to = InstantaneousAction('turn_to', s=Satellite, d_new=Direction, d_prev=Direction)
    s, d_new, d_prev = turn_to.parameter('s'), turn_to.parameter('d_new'), turn_to.parameter('d_prev')
    turn_to.add_precondition(Equals(pointing_at(s), d_prev))
    turn_to.add_precondition(Not(Equals(d_new, d_prev)))
    turn_to.add_effect(pointing_at(s), d_new)
    p.add_action(turn_to)

    # Action: switch_on(?i, ?s)
    switch_on = InstantaneousAction('switch_on', i=Instrument, s=Satellite)
    i, s = switch_on.parameter('i'), switch_on.parameter('s')
    switch_on.add_precondition(Equals(on_board(i), s))
    switch_on.add_precondition(power_avail(s))
    switch_on.add_effect(power_on(i), True)
    switch_on.add_effect(calibrated(i), False)
    switch_on.add_effect(power_avail(s), False)
    p.add_action(switch_on)

    # Action: switch_off(?i, ?s)
    switch_off = InstantaneousAction('switch_off', i=Instrument, s=Satellite)
    i, s = switch_off.parameter('i'), switch_off.parameter('s')
    switch_off.add_precondition(Equals(on_board(i), s))
    switch_off.add_precondition(power_on(i))
    switch_off.add_effect(power_on(i), False)
    switch_off.add_effect(power_avail(s), True)
    p.add_action(switch_off)

    # Action: calibrate(?s, ?i, ?d)
    calibrate = InstantaneousAction('calibrate', s=Satellite, i=Instrument, d=Direction)
    s, i, d = calibrate.parameter('s'), calibrate.parameter('i'), calibrate.parameter('d')
    calibrate.add_precondition(Equals(on_board(i), s))
    calibrate.add_precondition(Equals(cal_target(i), d))
    calibrate.add_precondition(Equals(pointing_at(s), d))
    calibrate.add_precondition(power_on(i))
    calibrate.add_effect(calibrated(i), True)
    p.add_action(calibrate)

    # Action: take_image(?s, ?d, ?i, ?m)
    take_image = InstantaneousAction('take_image', s=Satellite, d=Direction, i=Instrument, m=Mode)
    s, d, i, m = take_image.parameter('s'), take_image.parameter('d'), take_image.parameter('i'), take_image.parameter('m')
    take_image.add_precondition(calibrated(i))
    take_image.add_precondition(Equals(on_board(i), s))
    take_image.add_precondition(SetMember(m, supports(i)))
    take_image.add_precondition(power_on(i))
    take_image.add_precondition(Equals(pointing_at(s), d))
    take_image.add_effect(have_image(d, m), True)
    p.add_action(take_image)

    # Goals
    p.add_goal(have_image(Target1, thermograph0))
    p.add_goal(have_image(Target2, thermograph0))

    return p
