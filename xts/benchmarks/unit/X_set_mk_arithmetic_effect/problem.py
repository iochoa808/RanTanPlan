"""Assigning a Python set of FNode expressions (arithmetic) as a whole-set effect. PDDL-XTS: X_set_mk_arithmetic_effect"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_set_mk_arithmetic_effect')

    val_t = IntType(0, 9)

    chain = Fluent('chain', SetType(val_t))
    p.add_fluent(chain, default_initial_value=set())
    p.set_initial_value(chain, {Int(3)})

    # The Python API accepts {n, Plus(n, Int(1))} as a valid parameterised set assignment
    # (unlike PDDL which rejects arithmetic inside set.mk literals).
    advance = InstantaneousAction('advance', n=val_t)
    n = advance.parameter('n')
    advance.add_precondition(LT(n, Int(8)))
    advance.add_precondition(SetMember(n, chain))
    advance.add_effect(chain, {n, Plus(n, Int(1))})
    p.add_action(advance)

    p.add_goal(SetMember(Int(4), chain))
    return p