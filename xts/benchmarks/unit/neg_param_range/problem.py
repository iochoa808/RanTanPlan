"""
Action parameter with negative lower bound: val ∈ [-3, 3].
IPAR enumerates set(-3)…set(3); goal result = -2 requires set(-2).
PDDL-XTS: neg_param_range
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('neg_param_range')

    val_t  = IntType(-3, 3)
    result = Fluent('result', val_t)
    p.add_fluent(result, default_initial_value=Int(0))
    p.set_initial_value(result, Int(0))

    set_act = InstantaneousAction('set', v=val_t)
    v = set_act.parameter('v')
    set_act.add_effect(result, v)
    p.add_action(set_act)

    p.add_goal(Equals(result, Int(-2)))
    return p