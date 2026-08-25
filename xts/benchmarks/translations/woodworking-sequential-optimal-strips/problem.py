"""
Woodworking XTS — cut a small part from a board then varnish it mauve.
XTS features: object fluents (surface-of, wood-of, treatment-of, colour-of, boardsize-of, goalsize-of).
Plan: 3 steps (load-highspeed-saw, cut-board-small, do-immersion-varnish).
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('simple_wood_prob_xts')

    # Types
    Acolour         = UserType('acolour')
    Awood           = UserType('awood')
    Woodobj         = UserType('woodobj')
    Machine         = UserType('machine')
    Surface         = UserType('surface')
    Treatmentstatus = UserType('treatmentstatus')
    Aboardsize      = UserType('aboardsize')
    Apartsize       = UserType('apartsize')
    HighspeedSaw    = UserType('highspeed_saw', father=Machine)
    Glazer          = UserType('glazer',        father=Machine)
    Grinder         = UserType('grinder',       father=Machine)
    ImmersionVarnisher = UserType('immersion_varnisher', father=Machine)
    Planer          = UserType('planer',        father=Machine)
    Saw             = UserType('saw',           father=Machine)
    SprayVarnisher  = UserType('spray_varnisher', father=Machine)
    Board           = UserType('board', father=Woodobj)
    Part            = UserType('part',  father=Woodobj)

    # Constants: these are declared in the domain (:constants ...).
    # In UP they become objects of their respective types.
    verysmooth       = Object('verysmooth',       Surface)
    smooth           = Object('smooth',           Surface)
    rough            = Object('rough',            Surface)
    varnished        = Object('varnished',        Treatmentstatus)
    glazed           = Object('glazed',           Treatmentstatus)
    untreated        = Object('untreated',        Treatmentstatus)
    colourfragments  = Object('colourfragments',  Treatmentstatus)
    natural          = Object('natural',          Acolour)
    small_s          = Object('small',            Apartsize)
    medium_s         = Object('medium',           Apartsize)
    large_s          = Object('large',            Apartsize)

    # Problem objects
    grinder0             = Object('grinder0',             Grinder)
    immersion_varnisher0 = Object('immersion-varnisher0', ImmersionVarnisher)
    highspeed_saw0       = Object('highspeed-saw0',       HighspeedSaw)
    mauve                = Object('mauve',                Acolour)
    beech                = Object('beech',                Awood)
    p0                   = Object('p0',                   Part)
    b0                   = Object('b0',                   Board)
    s0                   = Object('s0',                   Aboardsize)
    s1                   = Object('s1',                   Aboardsize)
    s2                   = Object('s2',                   Aboardsize)
    p.add_objects([verysmooth, smooth, rough,
                   varnished, glazed, untreated, colourfragments,
                   natural, small_s, medium_s, large_s,
                   grinder0, immersion_varnisher0, highspeed_saw0,
                   mauve, beech, p0, b0, s0, s1, s2])

    # Boolean predicates
    unused       = Fluent('unused', obj=Part)
    available    = Fluent('available', obj=Woodobj)
    boardsize_successor = Fluent('boardsize_successor', size1=Aboardsize, size2=Aboardsize)
    in_highspeed_saw    = Fluent('in_highspeed_saw', b=Board, m=HighspeedSaw)
    empty_saw    = Fluent('empty_saw', m=HighspeedSaw)
    has_colour   = Fluent('has_colour', machine=Machine, colour=Acolour)
    contains_part = Fluent('contains_part', b=Board, part=Part)
    grind_treatment_change = Fluent('grind_treatment_change', old=Treatmentstatus, new=Treatmentstatus)
    is_smooth    = Fluent('is_smooth', surface=Surface)
    for f in [unused, available, boardsize_successor, in_highspeed_saw,
              empty_saw, has_colour, contains_part, grind_treatment_change, is_smooth]:
        p.add_fluent(f, default_initial_value=False)

    # Object fluents
    surface_of   = Fluent('surface_of',   Surface,         o=Woodobj)
    wood_of      = Fluent('wood_of',      Awood,           o=Woodobj)
    treatment_of = Fluent('treatment_of', Treatmentstatus, p_=Part)
    colour_of    = Fluent('colour_of',    Acolour,         p_=Part)
    boardsize_of = Fluent('boardsize_of', Aboardsize,      b=Board)
    goalsize_of  = Fluent('goalsize_of',  Apartsize,       p_=Part)
    for f in [surface_of, wood_of, treatment_of, colour_of, boardsize_of, goalsize_of]:
        p.add_fluent(f)

    # Initial state — domain predicates (grind-treatment-change, is-smooth, etc.)
    p.set_initial_value(grind_treatment_change(varnished, colourfragments), True)
    p.set_initial_value(grind_treatment_change(glazed, untreated), True)
    p.set_initial_value(grind_treatment_change(untreated, untreated), True)
    p.set_initial_value(grind_treatment_change(colourfragments, untreated), True)
    p.set_initial_value(is_smooth(smooth), True)
    p.set_initial_value(is_smooth(verysmooth), True)
    p.set_initial_value(boardsize_successor(s0, s1), True)
    p.set_initial_value(boardsize_successor(s1, s2), True)
    p.set_initial_value(has_colour(immersion_varnisher0, mauve), True)
    p.set_initial_value(empty_saw(highspeed_saw0), True)
    p.set_initial_value(unused(p0), True)

    # Object fluent initial values — part p0 placeholders
    p.set_initial_value(goalsize_of(p0), small_s)
    p.set_initial_value(surface_of(p0), smooth)
    p.set_initial_value(wood_of(p0), beech)
    p.set_initial_value(treatment_of(p0), untreated)
    p.set_initial_value(colour_of(p0), natural)

    # Board b0
    p.set_initial_value(boardsize_of(b0), s2)
    p.set_initial_value(wood_of(b0), beech)
    p.set_initial_value(surface_of(b0), smooth)
    p.set_initial_value(available(b0), True)

    # Actions

    # do-immersion-varnish(?x - part, ?m - immersion-varnisher, ?newcolour - acolour)
    do_iv = InstantaneousAction('do_immersion_varnish', x=Part, m=ImmersionVarnisher, newcolour=Acolour)
    x, m, newcolour = do_iv.parameter('x'), do_iv.parameter('m'), do_iv.parameter('newcolour')
    do_iv.add_precondition(available(x))
    do_iv.add_precondition(has_colour(m, newcolour))
    do_iv.add_precondition(is_smooth(surface_of(x)))
    do_iv.add_precondition(Equals(treatment_of(x), untreated))
    do_iv.add_effect(treatment_of(x), varnished)
    do_iv.add_effect(colour_of(x), newcolour)
    p.add_action(do_iv)

    # do-spray-varnish(?x - part, ?m - spray-varnisher, ?newcolour - acolour)
    do_sv = InstantaneousAction('do_spray_varnish', x=Part, m=SprayVarnisher, newcolour=Acolour)
    x, m, newcolour = do_sv.parameter('x'), do_sv.parameter('m'), do_sv.parameter('newcolour')
    do_sv.add_precondition(available(x))
    do_sv.add_precondition(has_colour(m, newcolour))
    do_sv.add_precondition(is_smooth(surface_of(x)))
    do_sv.add_precondition(Equals(treatment_of(x), untreated))
    do_sv.add_effect(treatment_of(x), varnished)
    do_sv.add_effect(colour_of(x), newcolour)
    p.add_action(do_sv)

    # do-glaze(?x - part, ?m - glazer, ?newcolour - acolour)
    do_glaze = InstantaneousAction('do_glaze', x=Part, m=Glazer, newcolour=Acolour)
    x, m, newcolour = do_glaze.parameter('x'), do_glaze.parameter('m'), do_glaze.parameter('newcolour')
    do_glaze.add_precondition(available(x))
    do_glaze.add_precondition(has_colour(m, newcolour))
    do_glaze.add_precondition(Equals(treatment_of(x), untreated))
    do_glaze.add_effect(treatment_of(x), glazed)
    do_glaze.add_effect(colour_of(x), newcolour)
    p.add_action(do_glaze)

    # do-grind(?x - part, ?m - grinder, ?newtreatment - treatmentstatus)
    do_grind = InstantaneousAction('do_grind', x=Part, m=Grinder, newtreatment=Treatmentstatus)
    x, m, newtreatment = do_grind.parameter('x'), do_grind.parameter('m'), do_grind.parameter('newtreatment')
    do_grind.add_precondition(available(x))
    do_grind.add_precondition(is_smooth(surface_of(x)))
    do_grind.add_precondition(grind_treatment_change(treatment_of(x), newtreatment))
    do_grind.add_effect(surface_of(x), verysmooth)
    do_grind.add_effect(treatment_of(x), newtreatment)
    do_grind.add_effect(colour_of(x), natural)
    p.add_action(do_grind)

    # do-plane(?x - part, ?m - planer)
    do_plane = InstantaneousAction('do_plane', x=Part, m=Planer)
    x, m = do_plane.parameter('x'), do_plane.parameter('m')
    do_plane.add_precondition(available(x))
    do_plane.add_effect(surface_of(x), smooth)
    do_plane.add_effect(treatment_of(x), untreated)
    do_plane.add_effect(colour_of(x), natural)
    p.add_action(do_plane)

    # load-highspeed-saw(?b - board, ?m - highspeed-saw)
    load_saw = InstantaneousAction('load_highspeed_saw', b=Board, m=HighspeedSaw)
    b, m = load_saw.parameter('b'), load_saw.parameter('m')
    load_saw.add_precondition(empty_saw(m))
    load_saw.add_precondition(available(b))
    load_saw.add_effect(available(b), False)
    load_saw.add_effect(empty_saw(m), False)
    load_saw.add_effect(in_highspeed_saw(b, m), True)
    p.add_action(load_saw)

    # unload-highspeed-saw(?b - board, ?m - highspeed-saw)
    unload_saw = InstantaneousAction('unload_highspeed_saw', b=Board, m=HighspeedSaw)
    b, m = unload_saw.parameter('b'), unload_saw.parameter('m')
    unload_saw.add_precondition(in_highspeed_saw(b, m))
    unload_saw.add_effect(available(b), True)
    unload_saw.add_effect(in_highspeed_saw(b, m), False)
    unload_saw.add_effect(empty_saw(m), True)
    p.add_action(unload_saw)

    # cut-board-small(?b, ?p, ?m, ?size_before, ?size_after)
    cut_small = InstantaneousAction('cut_board_small',
                                    b=Board, part=Part, m=HighspeedSaw,
                                    size_before=Aboardsize, size_after=Aboardsize)
    b, part, m, sb, sa = (cut_small.parameter(n)
                          for n in ('b', 'part', 'm', 'size_before', 'size_after'))
    cut_small.add_precondition(unused(part))
    cut_small.add_precondition(Equals(goalsize_of(part), small_s))
    cut_small.add_precondition(in_highspeed_saw(b, m))
    cut_small.add_precondition(Equals(boardsize_of(b), sb))
    cut_small.add_precondition(boardsize_successor(sa, sb))
    cut_small.add_effect(unused(part), False)
    cut_small.add_effect(available(part), True)
    cut_small.add_effect(wood_of(part), wood_of(b))
    cut_small.add_effect(surface_of(part), surface_of(b))
    cut_small.add_effect(colour_of(part), natural)
    cut_small.add_effect(treatment_of(part), untreated)
    cut_small.add_effect(boardsize_of(b), sa)
    p.add_action(cut_small)

    # cut-board-medium(?b, ?p, ?m, ?size_before, ?s1, ?size_after)
    cut_medium = InstantaneousAction('cut_board_medium',
                                     b=Board, part=Part, m=HighspeedSaw,
                                     size_before=Aboardsize, s1=Aboardsize, size_after=Aboardsize)
    b, part, m, sb, s1_p, sa = (cut_medium.parameter(n)
                                 for n in ('b', 'part', 'm', 'size_before', 's1', 'size_after'))
    cut_medium.add_precondition(unused(part))
    cut_medium.add_precondition(Equals(goalsize_of(part), medium_s))
    cut_medium.add_precondition(in_highspeed_saw(b, m))
    cut_medium.add_precondition(Equals(boardsize_of(b), sb))
    cut_medium.add_precondition(boardsize_successor(sa, s1_p))
    cut_medium.add_precondition(boardsize_successor(s1_p, sb))
    cut_medium.add_effect(unused(part), False)
    cut_medium.add_effect(available(part), True)
    cut_medium.add_effect(wood_of(part), wood_of(b))
    cut_medium.add_effect(surface_of(part), surface_of(b))
    cut_medium.add_effect(colour_of(part), natural)
    cut_medium.add_effect(treatment_of(part), untreated)
    cut_medium.add_effect(boardsize_of(b), sa)
    p.add_action(cut_medium)

    # cut-board-large(?b, ?p, ?m, ?size_before, ?s1, ?s2, ?size_after)
    cut_large = InstantaneousAction('cut_board_large',
                                    b=Board, part=Part, m=HighspeedSaw,
                                    size_before=Aboardsize, s1=Aboardsize,
                                    s2_p=Aboardsize, size_after=Aboardsize)
    b, part, m, sb, s1_p, s2_p, sa = (cut_large.parameter(n)
                                       for n in ('b', 'part', 'm', 'size_before', 's1', 's2_p', 'size_after'))
    cut_large.add_precondition(unused(part))
    cut_large.add_precondition(Equals(goalsize_of(part), large_s))
    cut_large.add_precondition(in_highspeed_saw(b, m))
    cut_large.add_precondition(Equals(boardsize_of(b), sb))
    cut_large.add_precondition(boardsize_successor(sa, s1_p))
    cut_large.add_precondition(boardsize_successor(s1_p, s2_p))
    cut_large.add_precondition(boardsize_successor(s2_p, sb))
    cut_large.add_effect(unused(part), False)
    cut_large.add_effect(available(part), True)
    cut_large.add_effect(wood_of(part), wood_of(b))
    cut_large.add_effect(surface_of(part), surface_of(b))
    cut_large.add_effect(colour_of(part), natural)
    cut_large.add_effect(treatment_of(part), untreated)
    cut_large.add_effect(boardsize_of(b), sa)
    p.add_action(cut_large)

    # do-saw-small(?b, ?p, ?m - saw, ?size_before, ?size_after)
    do_saw_small = InstantaneousAction('do_saw_small',
                                       b=Board, part=Part, m=Saw,
                                       size_before=Aboardsize, size_after=Aboardsize)
    b, part, m, sb, sa = (do_saw_small.parameter(n)
                           for n in ('b', 'part', 'm', 'size_before', 'size_after'))
    do_saw_small.add_precondition(unused(part))
    do_saw_small.add_precondition(Equals(goalsize_of(part), small_s))
    do_saw_small.add_precondition(available(b))
    do_saw_small.add_precondition(Equals(boardsize_of(b), sb))
    do_saw_small.add_precondition(boardsize_successor(sa, sb))
    do_saw_small.add_effect(unused(part), False)
    do_saw_small.add_effect(available(part), True)
    do_saw_small.add_effect(wood_of(part), wood_of(b))
    do_saw_small.add_effect(surface_of(part), surface_of(b))
    do_saw_small.add_effect(colour_of(part), natural)
    do_saw_small.add_effect(treatment_of(part), untreated)
    do_saw_small.add_effect(boardsize_of(b), sa)
    p.add_action(do_saw_small)

    # Goals
    p.add_goal(available(p0))
    p.add_goal(Equals(colour_of(p0), mauve))
    p.add_goal(Equals(wood_of(p0), beech))
    p.add_goal(Equals(treatment_of(p0), varnished))

    return p
