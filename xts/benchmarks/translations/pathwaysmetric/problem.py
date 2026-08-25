"""
Pathways-Metric XTS — produce pCAF-p300 complex.
XTS features: bounded integers (available, need, prod, num-subs).
Plan: 5 steps (choose x2, initialize x2, associate x1).
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('Simple_Pathways_Metric_xts')

    Molecule = UserType('molecule')
    Simple   = UserType('simple',  father=Molecule)
    Complex  = UserType('complex', father=Molecule)

    qty_t = IntType(0, 3)

    # Boolean predicates
    association_reaction = Fluent('association_reaction', x1=Molecule, x2=Molecule, x3=Complex)
    catalyzed_association_reaction = Fluent('catalyzed_association_reaction', x1=Molecule, x2=Molecule, x3=Complex)
    catalyzed_self_association_reaction = Fluent('catalyzed_self_association_reaction', x1=Molecule, x3=Complex)
    synthesis_reaction = Fluent('synthesis_reaction', x1=Molecule, x2=Molecule)
    possible = Fluent('possible', x=Molecule)
    chosen   = Fluent('chosen', x=Simple)
    for f in [association_reaction, catalyzed_association_reaction,
              catalyzed_self_association_reaction, synthesis_reaction, possible, chosen]:
        p.add_fluent(f, default_initial_value=False)

    # Bounded int fluents
    available = Fluent('available', qty_t, x=Molecule)
    need_assoc = Fluent('need_for_association', qty_t, x1=Molecule, x2=Molecule, x3=Complex)
    need_cat_assoc = Fluent('need_for_catalyzed_association', qty_t, x1=Molecule, x2=Molecule, x3=Complex)
    need_cat_self = Fluent('need_for_catalyzed_self_association', qty_t, x1=Molecule, x3=Complex)
    need_synth = Fluent('need_for_synthesis', qty_t, x1=Molecule, x2=Molecule)
    prod_assoc = Fluent('prod_by_association', qty_t, x1=Molecule, x2=Molecule, x3=Complex)
    prod_cat_assoc = Fluent('prod_by_catalyzed_association', qty_t, x1=Molecule, x2=Molecule, x3=Complex)
    prod_cat_self = Fluent('prod_by_catalyzed_self_association', qty_t, x1=Molecule, x3=Complex)
    prod_synth = Fluent('prod_by_synthesis', qty_t, x1=Molecule, x2=Molecule)
    num_subs = Fluent('num_subs', qty_t)
    for f in [available, need_assoc, need_cat_assoc, need_cat_self, need_synth,
              prod_assoc, prod_cat_assoc, prod_cat_self, prod_synth, num_subs]:
        p.add_fluent(f, default_initial_value=0)

    # Objects
    pCAF      = Object('pCAF',      Simple)
    p300      = Object('p300',      Simple)
    pCAF_p300 = Object('pCAF-p300', Complex)
    p.add_objects([pCAF, p300, pCAF_p300])

    all_molecules = [pCAF, p300, pCAF_p300]
    all_complexes = [pCAF_p300]

    # Initialize available for all molecules
    for m in all_molecules:
        p.set_initial_value(available(m), Int(0))

    # Initialize all combinations of 2-mol-1-complex fluents to 0
    for m1 in all_molecules:
        for m2 in all_molecules:
            for c in all_complexes:
                p.set_initial_value(need_assoc(m1, m2, c), Int(0))
                p.set_initial_value(need_cat_assoc(m1, m2, c), Int(0))
                p.set_initial_value(prod_assoc(m1, m2, c), Int(0))
                p.set_initial_value(prod_cat_assoc(m1, m2, c), Int(0))
        for c in all_complexes:
            p.set_initial_value(need_cat_self(m1, c), Int(0))
            p.set_initial_value(prod_cat_self(m1, c), Int(0))
        for m2 in all_molecules:
            p.set_initial_value(need_synth(m1, m2), Int(0))
            p.set_initial_value(prod_synth(m1, m2), Int(0))

    p.set_initial_value(num_subs, Int(0))

    # Problem-specific initial values
    p.set_initial_value(possible(pCAF), True)
    p.set_initial_value(possible(p300), True)
    p.set_initial_value(association_reaction(pCAF, p300, pCAF_p300), True)
    p.set_initial_value(need_assoc(pCAF, p300, pCAF_p300), Int(1))
    p.set_initial_value(need_assoc(p300, pCAF, pCAF_p300), Int(1))
    p.set_initial_value(prod_assoc(pCAF, p300, pCAF_p300), Int(1))

    # Action: choose(?x - simple)
    choose = InstantaneousAction('choose', x=Simple)
    x = choose.parameter('x')
    choose.add_precondition(possible(x))
    choose.add_precondition(LT(num_subs, Int(3)))
    choose.add_effect(chosen(x), True)
    choose.add_effect(possible(x), False)
    choose.add_effect(num_subs, Plus(num_subs, Int(1)))
    p.add_action(choose)

    # Action: initialize(?x - simple)
    initialize = InstantaneousAction('initialize', x=Simple)
    x = initialize.parameter('x')
    initialize.add_precondition(chosen(x))
    initialize.add_precondition(LT(available(x), Int(3)))
    initialize.add_effect(available(x), Plus(available(x), Int(1)))
    p.add_action(initialize)

    # Action: associate(?x1 ?x2 - molecule ?x3 - complex)
    associate = InstantaneousAction('associate', x1=Molecule, x2=Molecule, x3=Complex)
    x1, x2, x3 = associate.parameter('x1'), associate.parameter('x2'), associate.parameter('x3')
    associate.add_precondition(GE(available(x1), need_assoc(x1, x2, x3)))
    associate.add_precondition(GE(available(x2), need_assoc(x2, x1, x3)))
    associate.add_precondition(association_reaction(x1, x2, x3))
    associate.add_precondition(LT(available(x3), Int(3)))
    associate.add_effect(available(x1), Minus(available(x1), need_assoc(x1, x2, x3)))
    associate.add_effect(available(x2), Minus(available(x2), need_assoc(x2, x1, x3)))
    associate.add_effect(available(x3), Plus(available(x3), prod_assoc(x1, x2, x3)))
    p.add_action(associate)

    # Action: associate-with-catalyze(?x1 ?x2 - molecule ?x3 - complex)
    assoc_cat = InstantaneousAction('associate_with_catalyze', x1=Molecule, x2=Molecule, x3=Complex)
    x1, x2, x3 = assoc_cat.parameter('x1'), assoc_cat.parameter('x2'), assoc_cat.parameter('x3')
    assoc_cat.add_precondition(GE(available(x1), need_cat_assoc(x1, x2, x3)))
    assoc_cat.add_precondition(GE(available(x2), need_cat_assoc(x2, x1, x3)))
    assoc_cat.add_precondition(catalyzed_association_reaction(x1, x2, x3))
    assoc_cat.add_precondition(LT(available(x3), Int(3)))
    assoc_cat.add_effect(available(x1), Minus(available(x1), need_cat_assoc(x1, x2, x3)))
    assoc_cat.add_effect(available(x3), Plus(available(x3), prod_cat_assoc(x1, x2, x3)))
    p.add_action(assoc_cat)

    # Action: self-associate-with-catalyze(?x1 - molecule ?x3 - complex)
    self_assoc = InstantaneousAction('self_associate_with_catalyze', x1=Molecule, x3=Complex)
    x1, x3 = self_assoc.parameter('x1'), self_assoc.parameter('x3')
    self_assoc.add_precondition(GE(available(x1), need_cat_self(x1, x3)))
    self_assoc.add_precondition(catalyzed_self_association_reaction(x1, x3))
    self_assoc.add_precondition(LT(available(x3), Int(3)))
    self_assoc.add_effect(available(x1), Minus(available(x1), need_cat_self(x1, x3)))
    self_assoc.add_effect(available(x3), Plus(available(x3), prod_cat_self(x1, x3)))
    p.add_action(self_assoc)

    # Action: synthesize(?x1 ?x2 - molecule)
    synthesize = InstantaneousAction('synthesize', x1=Molecule, x2=Molecule)
    x1, x2 = synthesize.parameter('x1'), synthesize.parameter('x2')
    synthesize.add_precondition(GE(available(x1), need_synth(x1, x2)))
    synthesize.add_precondition(synthesis_reaction(x1, x2))
    synthesize.add_precondition(LT(available(x2), Int(3)))
    synthesize.add_effect(available(x2), Plus(available(x2), prod_synth(x1, x2)))
    p.add_action(synthesize)

    # Goal
    p.add_goal(GE(available(pCAF_p300), Int(1)))

    return p
