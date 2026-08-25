"""
Token select — assign object-typed fluent from an array read.
PDDL-XTS: scalar_from_array
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('token_select')

    Token = UserType('Token')
    tok_a = Object('tok_a', Token)
    tok_b = Object('tok_b', Token)
    tok_c = Object('tok_c', Token)
    p.add_objects([tok_a, tok_b, tok_c])

    stock  = Fluent('stock',  ArrayType(3, Token))
    picked = Fluent('picked', Token)
    p.add_fluent(stock)
    p.add_fluent(picked, default_initial_value=tok_a)

    p.set_initial_value(stock,  [tok_b, tok_a, tok_c])
    p.set_initial_value(picked, tok_a)

    for idx in range(3):
        act = InstantaneousAction(f'pick_{idx}', t=Token)
        t = act.parameter('t')
        act.add_precondition(Equals(stock[Int(idx)], t))
        act.add_effect(picked, t)
        p.add_action(act)

    p.add_goal(Equals(picked, tok_c))
    return p
