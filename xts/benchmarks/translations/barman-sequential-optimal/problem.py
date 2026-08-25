"""
barman: make cocktail1 (ingredient1 + ingredient2) in shot1.
Uses: shaker1, shot1, shot2, left/right hands, dispenser1/2.
PDDL-XTS: barman-sequential-optimal
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem("simple-barman-xts")

    # Types (hierarchy)
    Beverage   = UserType("beverage")
    Ingredient = UserType("ingredient", father=Beverage)
    Cocktail   = UserType("cocktail",   father=Beverage)
    Container  = UserType("container")
    Shot       = UserType("shot",   father=Container)
    Shaker     = UserType("shaker", father=Container)
    Hand       = UserType("hand")
    Dispenser  = UserType("dispenser")
    lvl_t      = IntType(0, 2)

    # Fluents
    ontable   = Fluent("ontable",   c=Container)
    holding   = Fluent("holding",   h=Hand, c=Container)
    handempty = Fluent("handempty", h=Hand)
    empty     = Fluent("empty",     c=Container)
    contains  = Fluent("contains",  c=Container, b=Beverage)
    clean     = Fluent("clean",     c=Container)
    used      = Fluent("used",      c=Container, b=Beverage)
    dispenses = Fluent("dispenses", d=Dispenser, i=Ingredient)
    unshaked  = Fluent("unshaked",  s=Shaker)
    shaked    = Fluent("shaked",    s=Shaker)
    cp1       = Fluent("cocktail-part1", c=Cocktail, i=Ingredient)
    cp2       = Fluent("cocktail-part2", c=Cocktail, i=Ingredient)
    slevel    = Fluent("shaker-level", lvl_t, s=Shaker)

    for f in [ontable, holding, handempty, empty, contains, clean, used,
              dispenses, unshaked, shaked, cp1, cp2]:
        p.add_fluent(f, default_initial_value=False)
    p.add_fluent(slevel, default_initial_value=0)

    # Objects
    shaker1      = Object("shaker1",     Shaker)
    left         = Object("left",        Hand)
    right        = Object("right",       Hand)
    shot1        = Object("shot1",       Shot)
    shot2        = Object("shot2",       Shot)
    ingredient1  = Object("ingredient1", Ingredient)
    ingredient2  = Object("ingredient2", Ingredient)
    cocktail1    = Object("cocktail1",   Cocktail)
    dispenser1   = Object("dispenser1",  Dispenser)
    dispenser2   = Object("dispenser2",  Dispenser)
    p.add_objects([shaker1, left, right, shot1, shot2,
                   ingredient1, ingredient2, cocktail1,
                   dispenser1, dispenser2])

    # Initial state
    p.set_initial_value(ontable(shaker1), True)
    p.set_initial_value(ontable(shot1),   True)
    p.set_initial_value(ontable(shot2),   True)
    p.set_initial_value(dispenses(dispenser1, ingredient1), True)
    p.set_initial_value(dispenses(dispenser2, ingredient2), True)
    p.set_initial_value(clean(shaker1), True)
    p.set_initial_value(clean(shot1),   True)
    p.set_initial_value(clean(shot2),   True)
    p.set_initial_value(empty(shaker1), True)
    p.set_initial_value(empty(shot1),   True)
    p.set_initial_value(empty(shot2),   True)
    p.set_initial_value(handempty(left),  True)
    p.set_initial_value(handempty(right), True)
    p.set_initial_value(slevel(shaker1),  Int(0))
    p.set_initial_value(cp1(cocktail1, ingredient1), True)
    p.set_initial_value(cp2(cocktail1, ingredient2), True)

    # ── Actions ──

    # grasp(?h, ?c): ontable+handempty -> holding
    grasp = InstantaneousAction("grasp", h=Hand, c=Container)
    h, c = grasp.parameter("h"), grasp.parameter("c")
    grasp.add_precondition(ontable(c))
    grasp.add_precondition(handempty(h))
    grasp.add_effect(ontable(c),   False)
    grasp.add_effect(handempty(h), False)
    grasp.add_effect(holding(h, c), True)
    p.add_action(grasp)

    # leave(?h, ?c): holding -> ontable
    leave = InstantaneousAction("leave", h=Hand, c=Container)
    h, c = leave.parameter("h"), leave.parameter("c")
    leave.add_precondition(holding(h, c))
    leave.add_effect(holding(h, c), False)
    leave.add_effect(handempty(h),  True)
    leave.add_effect(ontable(c),    True)
    p.add_action(leave)

    # fill-shot(?s, ?i, ?h1, ?h2, ?d)
    fill_shot = InstantaneousAction("fill-shot",
                                    s=Shot, i=Ingredient, h1=Hand, h2=Hand, d=Dispenser)
    s, i, h1, h2, d = (fill_shot.parameter(n) for n in ("s","i","h1","h2","d"))
    fill_shot.add_precondition(holding(h1, s))
    fill_shot.add_precondition(handempty(h2))
    fill_shot.add_precondition(dispenses(d, i))
    fill_shot.add_precondition(empty(s))
    fill_shot.add_precondition(clean(s))
    fill_shot.add_effect(empty(s),      False)
    fill_shot.add_effect(contains(s, i), True)
    fill_shot.add_effect(clean(s),      False)
    fill_shot.add_effect(used(s, i),    True)
    p.add_action(fill_shot)

    # refill-shot(?s, ?i, ?h1, ?h2, ?d)
    refill_shot = InstantaneousAction("refill-shot",
                                      s=Shot, i=Ingredient, h1=Hand, h2=Hand, d=Dispenser)
    s, i, h1, h2, d = (refill_shot.parameter(n) for n in ("s","i","h1","h2","d"))
    refill_shot.add_precondition(holding(h1, s))
    refill_shot.add_precondition(handempty(h2))
    refill_shot.add_precondition(dispenses(d, i))
    refill_shot.add_precondition(empty(s))
    refill_shot.add_precondition(used(s, i))
    refill_shot.add_effect(empty(s),       False)
    refill_shot.add_effect(contains(s, i), True)
    p.add_action(refill_shot)

    # empty-shot(?h, ?p, ?b)
    empty_shot = InstantaneousAction("empty-shot", h=Hand, s=Shot, b=Beverage)
    h, s, b = (empty_shot.parameter(n) for n in ("h","s","b"))
    empty_shot.add_precondition(holding(h, s))
    empty_shot.add_precondition(contains(s, b))
    empty_shot.add_effect(contains(s, b), False)
    empty_shot.add_effect(empty(s),        True)
    p.add_action(empty_shot)

    # clean-shot(?s, ?b, ?h1, ?h2)
    clean_shot = InstantaneousAction("clean-shot", s=Shot, b=Beverage, h1=Hand, h2=Hand)
    s, b, h1, h2 = (clean_shot.parameter(n) for n in ("s","b","h1","h2"))
    clean_shot.add_precondition(holding(h1, s))
    clean_shot.add_precondition(handempty(h2))
    clean_shot.add_precondition(empty(s))
    clean_shot.add_precondition(used(s, b))
    clean_shot.add_effect(used(s, b), False)
    clean_shot.add_effect(clean(s),   True)
    p.add_action(clean_shot)

    # pour-shot-to-clean-shaker(?s, ?i, ?d, ?h1)
    pour_clean = InstantaneousAction("pour-shot-to-clean-shaker",
                                     s=Shot, i=Ingredient, d=Shaker, h1=Hand)
    s, i, d, h1 = (pour_clean.parameter(n) for n in ("s","i","d","h1"))
    pour_clean.add_precondition(holding(h1, s))
    pour_clean.add_precondition(contains(s, i))
    pour_clean.add_precondition(empty(d))
    pour_clean.add_precondition(clean(d))
    pour_clean.add_precondition(LT(slevel(d), Int(2)))
    pour_clean.add_effect(contains(s, i),  False)
    pour_clean.add_effect(empty(s),         True)
    pour_clean.add_effect(contains(d, i),   True)
    pour_clean.add_effect(empty(d),         False)
    pour_clean.add_effect(clean(d),         False)
    pour_clean.add_effect(unshaked(d),      True)
    pour_clean.add_effect(slevel(d), Plus(slevel(d), Int(1)))
    p.add_action(pour_clean)

    # pour-shot-to-used-shaker(?s, ?i, ?d, ?h1)
    pour_used = InstantaneousAction("pour-shot-to-used-shaker",
                                    s=Shot, i=Ingredient, d=Shaker, h1=Hand)
    s, i, d, h1 = (pour_used.parameter(n) for n in ("s","i","d","h1"))
    pour_used.add_precondition(holding(h1, s))
    pour_used.add_precondition(contains(s, i))
    pour_used.add_precondition(unshaked(d))
    pour_used.add_precondition(LT(slevel(d), Int(2)))
    pour_used.add_effect(contains(s, i), False)
    pour_used.add_effect(contains(d, i), True)
    pour_used.add_effect(empty(s),        True)
    pour_used.add_effect(slevel(d), Plus(slevel(d), Int(1)))
    p.add_action(pour_used)

    # empty-shaker(?h, ?s, ?b)
    empty_shaker = InstantaneousAction("empty-shaker", h=Hand, s=Shaker, b=Cocktail)
    h, s, b = (empty_shaker.parameter(n) for n in ("h","s","b"))
    empty_shaker.add_precondition(holding(h, s))
    empty_shaker.add_precondition(contains(s, b))
    empty_shaker.add_precondition(shaked(s))
    empty_shaker.add_effect(shaked(s),     False)
    empty_shaker.add_effect(slevel(s),     Int(0))
    empty_shaker.add_effect(contains(s, b), False)
    empty_shaker.add_effect(empty(s),       True)
    p.add_action(empty_shaker)

    # clean-shaker(?h1, ?h2, ?s)
    clean_shaker = InstantaneousAction("clean-shaker", h1=Hand, h2=Hand, s=Shaker)
    h1, h2, s = (clean_shaker.parameter(n) for n in ("h1","h2","s"))
    clean_shaker.add_precondition(holding(h1, s))
    clean_shaker.add_precondition(handempty(h2))
    clean_shaker.add_precondition(empty(s))
    clean_shaker.add_effect(clean(s), True)
    p.add_action(clean_shaker)

    # shake(?b, ?d1, ?d2, ?s, ?h1, ?h2)
    shake = InstantaneousAction("shake",
                                b=Cocktail, d1=Ingredient, d2=Ingredient,
                                s=Shaker, h1=Hand, h2=Hand)
    b, d1, d2, s, h1, h2 = (shake.parameter(n) for n in ("b","d1","d2","s","h1","h2"))
    shake.add_precondition(holding(h1, s))
    shake.add_precondition(handempty(h2))
    shake.add_precondition(contains(s, d1))
    shake.add_precondition(contains(s, d2))
    shake.add_precondition(cp1(b, d1))
    shake.add_precondition(cp2(b, d2))
    shake.add_precondition(unshaked(s))
    shake.add_effect(unshaked(s),    False)
    shake.add_effect(contains(s, d1), False)
    shake.add_effect(contains(s, d2), False)
    shake.add_effect(shaked(s),      True)
    shake.add_effect(contains(s, b), True)
    p.add_action(shake)

    # pour-shaker-to-shot(?b, ?d, ?h, ?s)
    pour_out = InstantaneousAction("pour-shaker-to-shot",
                                   b=Beverage, d=Shot, h=Hand, s=Shaker)
    b, d, h, s = (pour_out.parameter(n) for n in ("b","d","h","s"))
    pour_out.add_precondition(holding(h, s))
    pour_out.add_precondition(shaked(s))
    pour_out.add_precondition(empty(d))
    pour_out.add_precondition(clean(d))
    pour_out.add_precondition(contains(s, b))
    pour_out.add_precondition(GE(slevel(s), Int(1)))
    pour_out.add_effect(clean(d),       False)
    pour_out.add_effect(empty(d),       False)
    pour_out.add_effect(contains(d, b), True)
    pour_out.add_effect(slevel(s), Minus(slevel(s), Int(1)))
    p.add_action(pour_out)

    # Goal
    p.add_goal(contains(shot1, cocktail1))
    return p
