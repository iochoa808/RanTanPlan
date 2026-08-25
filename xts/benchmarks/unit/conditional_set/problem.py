"""
Conditional set — admit item into pool only when not quarantined.
PDDL-XTS: conditional_set
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('cond_set')

    Item = UserType('Item')
    alpha = Object('alpha', Item)
    beta  = Object('beta',  Item)
    gamma = Object('gamma', Item)
    p.add_objects([alpha, beta, gamma])

    quarantined = Fluent('quarantined', x=Item)
    pool        = Fluent('pool',        SetType(Item))
    p.add_fluent(quarantined, default_initial_value=False)
    p.add_fluent(pool,        default_initial_value=set())

    p.set_initial_value(pool, set())
    p.set_initial_value(quarantined(gamma), True)

    admit = InstantaneousAction('admit', x=Item)
    x = admit.parameter('x')
    admit.add_precondition(Not(SetMember(x, pool)))
    admit.add_effect(pool, SetAdd(x, pool), condition=Not(quarantined(x)))
    p.add_action(admit)

    p.add_goal(SetMember(alpha, pool))
    p.add_goal(SetMember(beta,  pool))
    return p
