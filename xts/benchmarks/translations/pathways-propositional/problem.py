"""
pathways-propositional: choose pCAF and p300, initialize both, associate
to get pCAF-p300 (available), then DUMMY-ACTION-1 sets goal1.
Plan: 6 steps.
PDDL-XTS: pathways-propositional
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem("Simple-Pathways-xts")

    Molecule = UserType("molecule")
    Simple   = UserType("simple",  father=Molecule)
    Complex  = UserType("complex", father=Molecule)
    budget_t = IntType(0, 2)

    # Fluents
    assoc_rx   = Fluent("association-reaction",          x1=Molecule, x2=Molecule, x3=Complex)
    cat_rx     = Fluent("catalyzed-association-reaction", x1=Molecule, x2=Molecule, x3=Complex)
    synth_rx   = Fluent("synthesis-reaction",            x1=Molecule, x2=Molecule)
    possible   = Fluent("possible",   x=Molecule)
    available  = Fluent("available",  x=Molecule)
    chosen     = Fluent("chosen",     s=Simple)
    goal1      = Fluent("goal1")
    num_subs   = Fluent("num-subs",  budget_t)

    for f in [assoc_rx, cat_rx, synth_rx, possible, available, chosen, goal1]:
        p.add_fluent(f, default_initial_value=False)
    p.add_fluent(num_subs, default_initial_value=0)

    # Domain constants + problem objects
    pCAF_p300    = Object("pCAF-p300",    Complex)
    pRbp1p2_AP2  = Object("pRbp1p2-AP2", Complex)
    SP1          = Object("SP1",          Simple)
    E2F13        = Object("E2F13",        Simple)
    pCAF         = Object("pCAF",         Simple)
    p300         = Object("p300",         Simple)
    SP1_E2F13    = Object("SP1-E2F13",   Complex)
    p.add_objects([pCAF_p300, pRbp1p2_AP2, SP1, E2F13, pCAF, p300, SP1_E2F13])

    # Initial state
    p.set_initial_value(possible(SP1),   True)
    p.set_initial_value(possible(E2F13), True)
    p.set_initial_value(possible(pCAF),  True)
    p.set_initial_value(possible(p300),  True)
    p.set_initial_value(assoc_rx(SP1, E2F13, SP1_E2F13),  True)
    p.set_initial_value(assoc_rx(pCAF, p300, pCAF_p300),  True)
    p.set_initial_value(num_subs, Int(0))

    # Actions
    # choose(?x): possible, not chosen, num-subs < 2
    choose = InstantaneousAction("choose", x=Simple)
    x = choose.parameter("x")
    choose.add_precondition(possible(x))
    choose.add_precondition(Not(chosen(x)))
    choose.add_precondition(LT(num_subs, Int(2)))
    choose.add_effect(chosen(x),   True)
    choose.add_effect(num_subs, Plus(num_subs, Int(1)))
    p.add_action(choose)

    # initialize(?x): chosen -> available
    initialize = InstantaneousAction("initialize", x=Simple)
    x = initialize.parameter("x")
    initialize.add_precondition(chosen(x))
    initialize.add_effect(available(x), True)
    p.add_action(initialize)

    # associate(?x1, ?x2, ?x3)
    associate = InstantaneousAction("associate", x1=Molecule, x2=Molecule, x3=Complex)
    x1, x2, x3 = (associate.parameter(n) for n in ("x1","x2","x3"))
    associate.add_precondition(assoc_rx(x1, x2, x3))
    associate.add_precondition(available(x1))
    associate.add_precondition(available(x2))
    associate.add_effect(available(x1), False)
    associate.add_effect(available(x2), False)
    associate.add_effect(available(x3), True)
    p.add_action(associate)

    # associate-with-catalyze(?x1, ?x2, ?x3)
    assoc_cat = InstantaneousAction("associate-with-catalyze",
                                    x1=Molecule, x2=Molecule, x3=Complex)
    x1, x2, x3 = (assoc_cat.parameter(n) for n in ("x1","x2","x3"))
    assoc_cat.add_precondition(cat_rx(x1, x2, x3))
    assoc_cat.add_precondition(available(x1))
    assoc_cat.add_precondition(available(x2))
    assoc_cat.add_effect(available(x1), False)
    assoc_cat.add_effect(available(x3), True)
    p.add_action(assoc_cat)

    # synthesize(?x1, ?x2)
    synthesize = InstantaneousAction("synthesize", x1=Molecule, x2=Molecule)
    x1, x2 = (synthesize.parameter(n) for n in ("x1","x2"))
    synthesize.add_precondition(synth_rx(x1, x2))
    synthesize.add_precondition(available(x1))
    synthesize.add_effect(available(x2), True)
    p.add_action(synthesize)

    # DUMMY-ACTION-1: or(available(pRbp1p2-AP2), available(pCAF-p300)) -> goal1
    dummy = InstantaneousAction("DUMMY-ACTION-1")
    dummy.add_precondition(Or(available(pRbp1p2_AP2), available(pCAF_p300)))
    dummy.add_effect(goal1, True)
    p.add_action(dummy)

    p.add_goal(goal1)
    return p
