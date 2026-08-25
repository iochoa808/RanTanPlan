"""
child-snack-sequential-optimal XTS. Object fluents: (tray-at ?t) - place, (child-at ?c) - place.
Expected: 7 steps.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem("simple_child_snack_xts")

    Child         = UserType("child")
    BreadPortion  = UserType("bread-portion")
    ContentPortion= UserType("content-portion")
    Sandwich      = UserType("sandwich")
    Tray          = UserType("tray")
    Place         = UserType("place")

    # Objects
    child1   = Object("child1",    Child)
    child2   = Object("child2",    Child)
    bread1   = Object("bread1",    BreadPortion)
    bread2   = Object("bread2",    BreadPortion)
    content1 = Object("content1",  ContentPortion)
    content2 = Object("content2",  ContentPortion)
    tray1    = Object("tray1",     Tray)
    kitchen  = Object("kitchen",   Place)
    table1   = Object("table1",    Place)
    sandw1   = Object("sandw1",    Sandwich)
    sandw2   = Object("sandw2",    Sandwich)
    p.add_objects([child1, child2, bread1, bread2, content1, content2,
                   tray1, kitchen, table1, sandw1, sandw2])

    # Boolean predicates
    at_kitchen_bread    = Fluent("at_kitchen_bread",    b=BreadPortion)
    at_kitchen_content  = Fluent("at_kitchen_content",  c=ContentPortion)
    at_kitchen_sandwich = Fluent("at_kitchen_sandwich", s=Sandwich)
    no_gluten_bread     = Fluent("no_gluten_bread",     b=BreadPortion)
    no_gluten_content   = Fluent("no_gluten_content",   c=ContentPortion)
    ontray              = Fluent("ontray",               s=Sandwich, t=Tray)
    no_gluten_sandwich  = Fluent("no_gluten_sandwich",  s=Sandwich)
    allergic_gluten     = Fluent("allergic_gluten",     c=Child)
    not_allergic_gluten = Fluent("not_allergic_gluten", c=Child)
    served              = Fluent("served",               c=Child)
    notexist            = Fluent("notexist",             s=Sandwich)
    for f in [at_kitchen_bread, at_kitchen_content, at_kitchen_sandwich,
              no_gluten_bread, no_gluten_content, ontray, no_gluten_sandwich,
              allergic_gluten, not_allergic_gluten, served, notexist]:
        p.add_fluent(f, default_initial_value=False)

    # Object fluents
    tray_at  = Fluent("tray-at",  Place, t=Tray)
    child_at = Fluent("child-at", Place, c=Child)
    p.add_fluent(tray_at)
    p.add_fluent(child_at)

    # Initial state
    p.set_initial_value(tray_at(tray1),    kitchen)
    p.set_initial_value(child_at(child1),  table1)
    p.set_initial_value(child_at(child2),  table1)
    p.set_initial_value(at_kitchen_bread(bread1),     True)
    p.set_initial_value(at_kitchen_bread(bread2),     True)
    p.set_initial_value(at_kitchen_content(content1), True)
    p.set_initial_value(at_kitchen_content(content2), True)
    p.set_initial_value(no_gluten_bread(bread2),      True)
    p.set_initial_value(no_gluten_content(content2),  True)
    p.set_initial_value(allergic_gluten(child1),      True)
    p.set_initial_value(not_allergic_gluten(child2),  True)
    p.set_initial_value(notexist(sandw1),             True)
    p.set_initial_value(notexist(sandw2),             True)

    # make_sandwich_no_gluten(?s, ?b, ?c)
    mkng = InstantaneousAction("make_sandwich_no_gluten", s=Sandwich, b=BreadPortion, c=ContentPortion)
    s_p, b_p, c_p = mkng.parameter("s"), mkng.parameter("b"), mkng.parameter("c")
    mkng.add_precondition(at_kitchen_bread(b_p))
    mkng.add_precondition(at_kitchen_content(c_p))
    mkng.add_precondition(no_gluten_bread(b_p))
    mkng.add_precondition(no_gluten_content(c_p))
    mkng.add_precondition(notexist(s_p))
    mkng.add_effect(at_kitchen_bread(b_p),     False)
    mkng.add_effect(at_kitchen_content(c_p),   False)
    mkng.add_effect(at_kitchen_sandwich(s_p),  True)
    mkng.add_effect(no_gluten_sandwich(s_p),   True)
    mkng.add_effect(notexist(s_p),             False)
    p.add_action(mkng)

    # make_sandwich(?s, ?b, ?c)
    mks = InstantaneousAction("make_sandwich", s=Sandwich, b=BreadPortion, c=ContentPortion)
    s_p, b_p, c_p = mks.parameter("s"), mks.parameter("b"), mks.parameter("c")
    mks.add_precondition(at_kitchen_bread(b_p))
    mks.add_precondition(at_kitchen_content(c_p))
    mks.add_precondition(notexist(s_p))
    mks.add_effect(at_kitchen_bread(b_p),    False)
    mks.add_effect(at_kitchen_content(c_p),  False)
    mks.add_effect(at_kitchen_sandwich(s_p), True)
    mks.add_effect(notexist(s_p),            False)
    p.add_action(mks)

    # put_on_tray(?s, ?t)
    pot = InstantaneousAction("put_on_tray", s=Sandwich, t=Tray)
    s_p, t_p = pot.parameter("s"), pot.parameter("t")
    pot.add_precondition(at_kitchen_sandwich(s_p))
    pot.add_precondition(Equals(tray_at(t_p), kitchen))
    pot.add_effect(at_kitchen_sandwich(s_p), False)
    pot.add_effect(ontray(s_p, t_p), True)
    p.add_action(pot)

    # serve_sandwich_no_gluten(?s, ?c, ?t)
    ssng = InstantaneousAction("serve_sandwich_no_gluten", s=Sandwich, c=Child, t=Tray)
    s_p, c_p, t_p = ssng.parameter("s"), ssng.parameter("c"), ssng.parameter("t")
    ssng.add_precondition(allergic_gluten(c_p))
    ssng.add_precondition(ontray(s_p, t_p))
    ssng.add_precondition(no_gluten_sandwich(s_p))
    ssng.add_precondition(Equals(child_at(c_p), tray_at(t_p)))
    ssng.add_effect(ontray(s_p, t_p), False)
    ssng.add_effect(served(c_p), True)
    p.add_action(ssng)

    # serve_sandwich(?s, ?c, ?t)
    ss = InstantaneousAction("serve_sandwich", s=Sandwich, c=Child, t=Tray)
    s_p, c_p, t_p = ss.parameter("s"), ss.parameter("c"), ss.parameter("t")
    ss.add_precondition(not_allergic_gluten(c_p))
    ss.add_precondition(ontray(s_p, t_p))
    ss.add_precondition(Equals(child_at(c_p), tray_at(t_p)))
    ss.add_effect(ontray(s_p, t_p), False)
    ss.add_effect(served(c_p), True)
    p.add_action(ss)

    # move_tray(?t, ?p2)
    mt = InstantaneousAction("move_tray", t=Tray, p2=Place)
    t_p, p2_p = mt.parameter("t"), mt.parameter("p2")
    mt.add_effect(tray_at(t_p), p2_p)
    p.add_action(mt)

    # Goal
    p.add_goal(served(child1))
    p.add_goal(served(child2))

    return p
