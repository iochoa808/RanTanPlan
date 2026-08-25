"""
scanalyzer-3d-sequential-optimal-strips XTS. Object fluent: (seg-of ?c) - segment.
Expected: 2 steps.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem("minimal_scanalyzer_xts")

    Segment = UserType("segment")
    Car     = UserType("car")

    car_a = Object("car-a", Car)
    car_b = Object("car-b", Car)
    seg_1 = Object("seg-1", Segment)
    seg_2 = Object("seg-2", Segment)
    p.add_objects([car_a, car_b, seg_1, seg_2])

    # Boolean predicates
    analyzed             = Fluent("analyzed",              c=Car)
    cycle2               = Fluent("CYCLE-2",               s1=Segment, s2=Segment)
    cycle2_with_analysis = Fluent("CYCLE-2-WITH-ANALYSIS", s1=Segment, s2=Segment)
    p.add_fluent(analyzed,             default_initial_value=False)
    p.add_fluent(cycle2,               default_initial_value=False)
    p.add_fluent(cycle2_with_analysis, default_initial_value=False)

    # Object fluent: (seg-of ?c) - segment
    seg_of = Fluent("seg-of", Segment, c=Car)
    p.add_fluent(seg_of)

    # Initial state
    p.set_initial_value(cycle2(seg_1, seg_2),               True)
    p.set_initial_value(cycle2_with_analysis(seg_1, seg_2), True)
    p.set_initial_value(seg_of(car_a), seg_1)
    p.set_initial_value(seg_of(car_b), seg_2)

    # analyze-2(?s1, ?s2, ?c1, ?c2)
    analyze2 = InstantaneousAction("analyze-2", s1=Segment, s2=Segment, c1=Car, c2=Car)
    s1_p, s2_p, c1_p, c2_p = (analyze2.parameter(n) for n in ("s1", "s2", "c1", "c2"))
    analyze2.add_precondition(cycle2_with_analysis(s1_p, s2_p))
    analyze2.add_precondition(Equals(seg_of(c1_p), s1_p))
    analyze2.add_precondition(Equals(seg_of(c2_p), s2_p))
    analyze2.add_effect(seg_of(c1_p), s2_p)
    analyze2.add_effect(seg_of(c2_p), s1_p)
    analyze2.add_effect(analyzed(c1_p), True)
    p.add_action(analyze2)

    # rotate-2(?s1, ?s2, ?c1, ?c2)
    rotate2 = InstantaneousAction("rotate-2", s1=Segment, s2=Segment, c1=Car, c2=Car)
    s1_p, s2_p, c1_p, c2_p = (rotate2.parameter(n) for n in ("s1", "s2", "c1", "c2"))
    rotate2.add_precondition(cycle2(s1_p, s2_p))
    rotate2.add_precondition(Equals(seg_of(c1_p), s1_p))
    rotate2.add_precondition(Equals(seg_of(c2_p), s2_p))
    rotate2.add_effect(seg_of(c1_p), s2_p)
    rotate2.add_effect(seg_of(c2_p), s1_p)
    p.add_action(rotate2)

    # Goal: both analyzed, back in original positions
    p.add_goal(analyzed(car_a))
    p.add_goal(analyzed(car_b))
    p.add_goal(Equals(seg_of(car_a), seg_1))
    p.add_goal(Equals(seg_of(car_b), seg_2))

    return p
