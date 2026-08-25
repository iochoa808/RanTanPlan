"""SetCardinality applied to a scalar integer fluent. PDDL-XTS: X_sem_cardinality_of_scalar"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_sem_cardinality_of_scalar')

    score_t = IntType(0, 9)

    score = Fluent('score', score_t)
    bonus = Fluent('bonus', score_t)
    p.add_fluent(score, default_initial_value=7)
    p.add_fluent(bonus, default_initial_value=0)

    # Error: SetCardinality requires a set argument; score is a scalar integer
    check_big = InstantaneousAction('check_big')
    check_big.add_precondition(GE(SetCardinality(score), 3))
    check_big.add_effect(bonus, 1)
    p.add_action(check_big)

    # Error: SetCardinality of scalar used in arithmetic
    award = InstantaneousAction('award')
    award.add_effect(bonus, Plus(SetCardinality(score), Int(1)))
    p.add_action(award)

    p.add_goal(Equals(bonus, 1))
    return p