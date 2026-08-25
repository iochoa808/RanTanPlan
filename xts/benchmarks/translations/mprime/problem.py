"""
Mystery-Prime XTS — abrasion must crave rice.
XTS features: bounded integers (locale, harmony), sets (eats-set).
Plan: 5 steps.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('mprime_xts')

    Food    = UserType('food')
    Emotion = UserType('emotion')
    Pleasure = UserType('pleasure', father=Emotion)
    Pain     = UserType('pain',     father=Emotion)

    lvl_t = IntType(0, 15)

    # Boolean predicates
    craves = Fluent('craves', v=Emotion, n=Food)
    fears  = Fluent('fears', c=Pain, v=Pleasure)
    p.add_fluent(craves, default_initial_value=False)
    p.add_fluent(fears,  default_initial_value=False)

    # Set fluent: (eats-set ?n) - foodset
    eats_set = Fluent('eats_set', SetType(Food), n=Food)
    p.add_fluent(eats_set, default_initial_value=set())

    # Bounded int fluents
    harmony = Fluent('harmony', lvl_t, v=Emotion)
    locale  = Fluent('locale',  lvl_t, n=Food)
    p.add_fluent(harmony, default_initial_value=0)
    p.add_fluent(locale,  default_initial_value=0)

    # Objects
    rice      = Object('rice',      Food)
    pear      = Object('pear',      Food)
    flounder  = Object('flounder',  Food)
    okra      = Object('okra',      Food)
    pork      = Object('pork',      Food)
    lamb      = Object('lamb',      Food)
    rest       = Object('rest',      Pleasure)
    hangover   = Object('hangover',  Pain)
    depression = Object('depression',Pain)
    abrasion   = Object('abrasion',  Pain)
    p.add_objects([rice, pear, flounder, okra, pork, lamb,
                   rest, hangover, depression, abrasion])

    # Initial state — eats-sets
    p.set_initial_value(eats_set(lamb),     {pork, flounder})
    p.set_initial_value(eats_set(pork),     {okra, lamb})
    p.set_initial_value(eats_set(okra),     {pear, pork})
    p.set_initial_value(eats_set(rice),     {rice, flounder, pear})
    p.set_initial_value(eats_set(flounder), {lamb, rice})
    p.set_initial_value(eats_set(pear),     {okra, rice})

    # locale values
    p.set_initial_value(locale(okra),     Int(6))
    p.set_initial_value(locale(pork),     Int(5))
    p.set_initial_value(locale(rice),     Int(1))
    p.set_initial_value(locale(pear),     Int(2))
    p.set_initial_value(locale(lamb),     Int(3))
    p.set_initial_value(locale(flounder), Int(4))

    # harmony
    p.set_initial_value(harmony(rest),       Int(3))
    p.set_initial_value(harmony(hangover),   Int(0))
    p.set_initial_value(harmony(depression), Int(0))
    p.set_initial_value(harmony(abrasion),   Int(0))

    # craves
    p.set_initial_value(craves(depression, flounder), True)
    p.set_initial_value(craves(abrasion,   pork),     True)
    p.set_initial_value(craves(rest,       pork),     True)
    p.set_initial_value(craves(hangover,   rice),     True)

    # Action: overcome(?c - pain, ?v - pleasure, ?n - food)
    overcome = InstantaneousAction('overcome', c=Pain, v=Pleasure, n=Food)
    c, v, n = overcome.parameter('c'), overcome.parameter('v'), overcome.parameter('n')
    overcome.add_precondition(craves(c, n))
    overcome.add_precondition(craves(v, n))
    overcome.add_precondition(GE(harmony(v), Int(1)))
    overcome.add_effect(craves(c, n), False)
    overcome.add_effect(fears(c, v), True)
    overcome.add_effect(harmony(v), Minus(harmony(v), Int(1)))
    p.add_action(overcome)

    # Action: feast(?v - pleasure, ?n1 ?n2 - food)
    feast = InstantaneousAction('feast', v=Pleasure, n1=Food, n2=Food)
    v, n1, n2 = feast.parameter('v'), feast.parameter('n1'), feast.parameter('n2')
    feast.add_precondition(craves(v, n1))
    feast.add_precondition(SetMember(n2, eats_set(n1)))
    feast.add_precondition(GE(locale(n1), Int(1)))
    feast.add_effect(craves(v, n1), False)
    feast.add_effect(craves(v, n2), True)
    feast.add_effect(locale(n1), Minus(locale(n1), Int(1)))
    p.add_action(feast)

    # Action: succumb(?c - pain, ?v - pleasure, ?n - food)
    succumb = InstantaneousAction('succumb', c=Pain, v=Pleasure, n=Food)
    c, v, n = succumb.parameter('c'), succumb.parameter('v'), succumb.parameter('n')
    succumb.add_precondition(fears(c, v))
    succumb.add_precondition(craves(v, n))
    succumb.add_effect(fears(c, v), False)
    succumb.add_effect(craves(c, n), True)
    succumb.add_effect(harmony(v), Plus(harmony(v), Int(1)))
    p.add_action(succumb)

    # Action: drink(?n1 ?n2 - food)
    drink = InstantaneousAction('drink', n1=Food, n2=Food)
    n1, n2 = drink.parameter('n1'), drink.parameter('n2')
    drink.add_precondition(GE(locale(n1), Int(1)))
    drink.add_effect(locale(n1), Minus(locale(n1), Int(1)))
    drink.add_effect(locale(n2), Plus(locale(n2), Int(1)))
    p.add_action(drink)

    # Goal
    p.add_goal(craves(abrasion, rice))

    return p
